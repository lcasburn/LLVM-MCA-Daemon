#ifndef LLVM_MCAD_BROKERS_BROKER_H
#define LLVM_MCAD_BROKERS_BROKER_H
#include "llvm/ADT/ArrayRef.h"
#include "llvm/MC/MCInst.h"
#include <utility>

namespace llvm {
namespace mcad {
// A simple interface for MCAWorker to fetch next MCInst
//
// Currently it's totally up to the Brokers to control their
// lifecycle. Client of this interface only cares about MCInsts.
struct Broker {
  // Broker should own the MCInst so only return the pointer
  //
  // Fetch MCInsts in batch. Size is the desired number of MCInsts
  // requested by the caller. When it's -1 the Broker will put until MCIS
  // is full. Of course, it's totally possible that Broker will only give out
  // MCInsts that are less than Size.
  // Note that for performance reason we're using mutable ArrayRef so the caller
  // should supply a fixed size array. And the Broker will always write from
  // index 0.
  // Return the number of MCInst put into the buffer, or -1 if no MCInst left
  virtual int fetch(MutableArrayRef<const MCInst*> MCIS, int Size = -1) {
    return -1;
  }

  // Region is similar to `CodeRegion` in the original llvm-mca. Basically
  // MCAD will create a separate MCA pipeline to analyze each Region.
  // If a Broker supports Region this method should return true and MCAD will
  // use `fetchRegion` method instead.
  virtual bool hasRegionFeature() const { return false; }

  // Similar to `fetch`, but returns the number of MCInst fetched and whether
  // the last element in MCIS is also the last instructions in the current Region.
  // Note that MCIS always aligned with the boundary of Region (i.e. the last
  // instruction of a Region will not be in the middle of MCIS)
  virtual std::pair<int, bool> fetchRegion(MutableArrayRef<const MCInst*> MCIS,
                                           int Size = -1) {
    return std::make_pair(-1, true);
  }

  virtual ~Broker() {}
};
} // end namespace mcad
} // end namespace llvm
#endif
