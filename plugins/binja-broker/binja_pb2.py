# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: binja.proto
# Protobuf Python Version: 4.25.0
"""Generated protocol buffer code."""
from google.protobuf import descriptor as _descriptor
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf import symbol_database as _symbol_database
from google.protobuf.internal import builder as _builder
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(b'\n\x0b\x62inja.proto\"g\n\x11\x42injaInstructions\x12\x33\n\x0binstruction\x18\x01 \x03(\x0b\x32\x1e.BinjaInstructions.Instruction\x1a\x1d\n\x0bInstruction\x12\x0e\n\x06opcode\x18\x01 \x01(\x0c\"j\n\x0b\x43ycleCounts\x12,\n\x0b\x63ycle_count\x18\x01 \x03(\x0b\x32\x17.CycleCounts.CycleCount\x1a-\n\nCycleCount\x12\r\n\x05ready\x18\x01 \x01(\x04\x12\x10\n\x08\x65xecuted\x18\x02 \x01(\x04\x32?\n\x05\x42inja\x12\x36\n\x12RequestCycleCounts\x12\x12.BinjaInstructions\x1a\x0c.CycleCountsb\x06proto3')

_globals = globals()
_builder.BuildMessageAndEnumDescriptors(DESCRIPTOR, _globals)
_builder.BuildTopDescriptorsAndMessages(DESCRIPTOR, 'binja_pb2', _globals)
if _descriptor._USE_C_DESCRIPTORS == False:
  DESCRIPTOR._options = None
  _globals['_BINJAINSTRUCTIONS']._serialized_start=15
  _globals['_BINJAINSTRUCTIONS']._serialized_end=118
  _globals['_BINJAINSTRUCTIONS_INSTRUCTION']._serialized_start=89
  _globals['_BINJAINSTRUCTIONS_INSTRUCTION']._serialized_end=118
  _globals['_CYCLECOUNTS']._serialized_start=120
  _globals['_CYCLECOUNTS']._serialized_end=226
  _globals['_CYCLECOUNTS_CYCLECOUNT']._serialized_start=181
  _globals['_CYCLECOUNTS_CYCLECOUNT']._serialized_end=226
  _globals['_BINJA']._serialized_start=228
  _globals['_BINJA']._serialized_end=291
# @@protoc_insertion_point(module_scope)