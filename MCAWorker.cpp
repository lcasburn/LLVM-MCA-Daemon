#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MCA/Context.h"
#include "llvm/MCA/InstrBuilder.h"
#include "llvm/MCA/Instruction.h"
#include "llvm/MCA/Pipeline.h"
#include "llvm/MCA/Stages/EntryStage.h"
#include "llvm/MCA/Stages/InstructionTables.h"
#include "llvm/MCA/Support.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/WithColor.h"
#include <string>
#include <system_error>

#include "MCAWorker.h"
#include "MCAViews/SummaryView.h"
#include "PipelinePrinter.h"

using namespace llvm;
using namespace mcad;

static cl::opt<bool>
  PrintJson("print-json", cl::desc("Export MCA analysis in JSON format"),
            cl::init(false));

static cl::opt<bool>
  TraceMCI("dump-trace-mc-inst", cl::desc("Dump collected MCInst in the trace"),
           cl::init(false));
static cl::opt<std::string>
  MCITraceFile("trace-mc-inst-output",
               cl::desc("Output to file for `-dump-trace-mc-inst`"
                        ". Print them to stdout otherwise"),
               cl::init("-"));

static cl::opt<bool>
  PreserveCallInst("use-call-inst",
                   cl::desc("Include call instruction in MCA"),
                   cl::init(false));

#define DEFAULT_MAX_NUM_PROCESSED 10000U
static cl::opt<unsigned>
  MaxNumProcessedInst("mca-max-chunk-size",
                      cl::desc("Max number of instructions processed at a time"),
                      cl::init(DEFAULT_MAX_NUM_PROCESSED));
#ifndef NDEBUG
static cl::opt<bool>
  DumpSourceMgrStats("dump-mca-sourcemgr-stats",
                     cl::Hidden, cl::init(false));
#endif

static cl::opt<unsigned>
  NumMCAIterations("mca-iteration",
                   cl::desc("Number of MCA simulation iteration"),
                   cl::init(1U));

MCAWorker::MCAWorker(const MCSubtargetInfo &TheSTI,
                     mca::Context &MCA,
                     const mca::PipelineOptions &PO,
                     mca::InstrBuilder &IB,
                     const MCInstrInfo &II,
                     MCInstPrinter &IP)
  : STI(TheSTI), MCAIB(IB), MCII(II), MIP(IP),
    TraceMIs(), GetTraceMISize([this]{ return TraceMIs.size(); }),
    GetRecycledInst([this](const mca::InstrDesc &Desc) -> mca::Instruction* {
                      if (RecycledInsts.count(&Desc)) {
                        auto &Insts = RecycledInsts[&Desc];
                        if (Insts.size()) {
                          mca::Instruction *I = *Insts.begin();
                          Insts.erase(Insts.cbegin());
                          return I;
                        }
                      }
                      return nullptr;
                    }),
    AddRecycledInst([this](mca::Instruction *I) {
                      const mca::InstrDesc &D = I->getDesc();
                      RecycledInsts[&D].insert(I);
                    }),
    Timers("MCABridge", "Time consumption in each MCABridge stages") {
  MCAIB.setInstRecycleCallback(GetRecycledInst);

  SrcMgr.setOnInstFreedCallback(AddRecycledInst);

  MCAPipeline = std::move(MCA.createDefaultPipeline(PO, SrcMgr));
  assert(MCAPipeline);

  MCAPipelinePrinter
    = std::make_unique<mca::PipelinePrinter>(*MCAPipeline,
                                             PrintJson ? mca::View::OK_JSON
                                                       : mca::View::OK_READABLE);
  const MCSchedModel &SM = STI.getSchedModel();
  MCAPipelinePrinter->addView(
    std::make_unique<mca::SummaryView>(SM, GetTraceMISize, 0U));
}

Error MCAWorker::run() {
  if (!TheBroker) {
    return llvm::createStringError(std::errc::invalid_argument,
                                   "No Broker is set");
  }

  raw_ostream *TraceOS = nullptr;
  std::unique_ptr<ToolOutputFile> TraceTOF;
  if (TraceMCI) {
    std::error_code EC;
    TraceTOF
      = std::make_unique<ToolOutputFile>(MCITraceFile, EC, sys::fs::OF_Text);
    if (EC) {
      errs() << "Failed to open trace file: " << EC.message() << "\n";
    } else {
      TraceOS = &TraceTOF->os();
    }
  }

  SmallVector<const MCInst*, DEFAULT_MAX_NUM_PROCESSED>
    TraceBuffer(MaxNumProcessedInst);

  bool Continue = true;
  while (Continue) {
    int Len = TheBroker->fetch(TraceBuffer);
    if (Len < 0) {
      Len = 0;
      SrcMgr.endOfStream();
      Continue = false;
    }
    ArrayRef<const MCInst*> TraceBufferSlice(TraceBuffer);
    TraceBufferSlice = TraceBufferSlice.take_front(Len);

    static Timer TheTimer("MCAInstrBuild", "MCA Build Instruction", Timers);
    {
      TimeRegion TR(TheTimer);

      // Convert MCInst to mca::Instruction
      for (const MCInst *OrigMCI : TraceBufferSlice) {
        TraceMIs.push_back(OrigMCI);
        const MCInst &MCI = *TraceMIs.back();
        const auto &MCID = MCII.get(MCI.getOpcode());
        // Always ignore return instruction since it's
        // not really meaningful
        if (MCID.isReturn()) continue;
        if (!PreserveCallInst)
          if (MCID.isCall())
            continue;

        if (TraceOS) {
          MIP.printInst(&MCI, 0, "", STI, *TraceOS);
          (*TraceOS) << "\n";
        }

        mca::Instruction *RecycledInst = nullptr;
        Expected<std::unique_ptr<mca::Instruction>> InstOrErr
          = MCAIB.createInstruction(MCI);
        if (!InstOrErr) {
          if (auto NewE = handleErrors(
                   InstOrErr.takeError(),
                   [&](const mca::RecycledInstErr &RC) {
                     RecycledInst = RC.getInst();
                   })) {
            return std::move(NewE);
          }
        }
        if (RecycledInst)
          SrcMgr.addRecycledInst(RecycledInst);
        else
          SrcMgr.addInst(std::move(InstOrErr.get()));
      }
    }

    if (auto E = runPipeline())
      return E;
  }

  if (TraceMCI && TraceTOF)
    TraceTOF->keep();

  return ErrorSuccess();
}

Error MCAWorker::runPipeline() {
  assert(MCAPipeline);
  static Timer TheTimer("RunMCAPipeline", "MCA Pipeline", Timers);
  TimeRegion TR(TheTimer);

  Expected<unsigned> Cycles = MCAPipeline->run();
  if (!Cycles) {
    if (!Cycles.errorIsA<mca::InstStreamPause>()) {
      return Cycles.takeError();
    } else {
      // Consume the error
      handleAllErrors(std::move(Cycles.takeError()),
                      [](const mca::InstStreamPause &PE) {});
    }
  }

  return ErrorSuccess();
}

void MCAWorker::printMCA(ToolOutputFile &OF) {
  MCAPipelinePrinter->printReport(OF.os());
  OF.keep();

#ifndef NDEBUG
  if (DumpSourceMgrStats)
    SrcMgr.printStatistic(
      dbgs() << "==== IncrementalSourceMgr Stats ====\n");
#endif
}
