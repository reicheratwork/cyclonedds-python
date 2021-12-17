"""
  Generated by Eclipse Cyclone DDS idlc Python Backend
  Cyclone DDS IDL version: v0.9.0
  Module: DDS.XTypes
  IDL file: ddsi_xt_typemap.idl

"""

from enum import auto
from typing import TYPE_CHECKING, Optional
from dataclasses import dataclass

import cyclonedds.idl as idl
import cyclonedds.idl.annotations as annotate
import cyclonedds.idl.types as types

# root module import for resolving types
import cyclonedds.idl._typesupport.DDS


@dataclass
@annotate.final
@annotate.autoid("sequential")
class TypeMapping(idl.IdlStruct, typename="DDS.XTypes.TypeMapping"):
    identifier_object_pair_minimal: types.sequence['cyclonedds.idl._typesupport.DDS.XTypes.TypeIdentifierTypeObjectPair']
    identifier_object_pair_complete: types.sequence['cyclonedds.idl._typesupport.DDS.XTypes.TypeIdentifierTypeObjectPair']
    identifier_complete_minimal: types.sequence['cyclonedds.idl._typesupport.DDS.XTypes.TypeIdentifierPair']


