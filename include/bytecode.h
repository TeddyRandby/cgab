//clang-format off
/*
  -------------- Zero Arguments -------------
*/
OP_CODE(NOP)
OP_CODE(SWAP)
OP_CODE(MATCH)
OP_CODE(CONCAT)
OP_CODE(STRINGIFY)
OP_CODE(PUSH_TRUE)
OP_CODE(PUSH_FALSE)
OP_CODE(PUSH_NULL)
OP_CODE(POP)
OP_CODE(POP_N)
OP_CODE(NOT)
OP_CODE(NEGATE)
OP_CODE(ADD)
OP_CODE(SUBTRACT)
OP_CODE(MULTIPLY)
OP_CODE(DIVIDE)
OP_CODE(MODULO)
OP_CODE(EQUAL)
OP_CODE(LESSER)
OP_CODE(LESSER_EQUAL)
OP_CODE(GREATER)
OP_CODE(GREATER_EQUAL)
OP_CODE(LOGICAL_AND)
OP_CODE(LOGICAL_OR)
OP_CODE(BITWISE_AND)
OP_CODE(BITWISE_OR)
OP_CODE(ASSERT)
OP_CODE(TYPE)
OP_CODE(STORE_LOCAL_0)
OP_CODE(STORE_LOCAL_1)
OP_CODE(STORE_LOCAL_2)
OP_CODE(STORE_LOCAL_3)
OP_CODE(STORE_LOCAL_4)
OP_CODE(STORE_LOCAL_5)
OP_CODE(STORE_LOCAL_6)
OP_CODE(STORE_LOCAL_7)
OP_CODE(STORE_LOCAL_8)
OP_CODE(POP_STORE_LOCAL_0)
OP_CODE(POP_STORE_LOCAL_1)
OP_CODE(POP_STORE_LOCAL_2)
OP_CODE(POP_STORE_LOCAL_3)
OP_CODE(POP_STORE_LOCAL_4)
OP_CODE(POP_STORE_LOCAL_5)
OP_CODE(POP_STORE_LOCAL_6)
OP_CODE(POP_STORE_LOCAL_7)
OP_CODE(POP_STORE_LOCAL_8)
OP_CODE(LOAD_LOCAL_0)
OP_CODE(LOAD_LOCAL_1)
OP_CODE(LOAD_LOCAL_2)
OP_CODE(LOAD_LOCAL_3)
OP_CODE(LOAD_LOCAL_4)
OP_CODE(LOAD_LOCAL_5)
OP_CODE(LOAD_LOCAL_6)
OP_CODE(LOAD_LOCAL_7)
OP_CODE(LOAD_LOCAL_8)
OP_CODE(STORE_UPVALUE_0)
OP_CODE(STORE_UPVALUE_1)
OP_CODE(STORE_UPVALUE_2)
OP_CODE(STORE_UPVALUE_3)
OP_CODE(STORE_UPVALUE_4)
OP_CODE(STORE_UPVALUE_5)
OP_CODE(STORE_UPVALUE_6)
OP_CODE(STORE_UPVALUE_7)
OP_CODE(STORE_UPVALUE_8)
OP_CODE(LOAD_UPVALUE_0)
OP_CODE(LOAD_UPVALUE_1)
OP_CODE(LOAD_UPVALUE_2)
OP_CODE(LOAD_UPVALUE_3)
OP_CODE(LOAD_UPVALUE_4)
OP_CODE(LOAD_UPVALUE_5)
OP_CODE(LOAD_UPVALUE_6)
OP_CODE(LOAD_UPVALUE_7)
OP_CODE(LOAD_UPVALUE_8)
OP_CODE(LOAD_CONST_UPVALUE_0)
OP_CODE(LOAD_CONST_UPVALUE_1)
OP_CODE(LOAD_CONST_UPVALUE_2)
OP_CODE(LOAD_CONST_UPVALUE_3)
OP_CODE(LOAD_CONST_UPVALUE_4)
OP_CODE(LOAD_CONST_UPVALUE_5)
OP_CODE(LOAD_CONST_UPVALUE_6)
OP_CODE(LOAD_CONST_UPVALUE_7)
OP_CODE(LOAD_CONST_UPVALUE_8)

OP_CODE(RETURN_1)
OP_CODE(RETURN_2)
OP_CODE(RETURN_3)
OP_CODE(RETURN_4)
OP_CODE(RETURN_5)
OP_CODE(RETURN_6)
OP_CODE(RETURN_7)
OP_CODE(RETURN_8)
OP_CODE(RETURN_9)
OP_CODE(RETURN_10)
OP_CODE(RETURN_11)
OP_CODE(RETURN_12)
OP_CODE(RETURN_13)
OP_CODE(RETURN_14)
OP_CODE(RETURN_15)
OP_CODE(RETURN_16)

/*
  -------------- Single Byte Arguments -------------
*/
OP_CODE(CONSTANT)
OP_CODE(LOAD_LOCAL)
OP_CODE(STORE_LOCAL)
OP_CODE(POP_STORE_LOCAL)
OP_CODE(LOAD_UPVALUE)
OP_CODE(STORE_UPVALUE)
OP_CODE(POP_STORE_UPVALUE)
OP_CODE(LOAD_CONST_UPVALUE)

OP_CODE(SPREAD)

OP_CODE(VARRETURN)

OP_CODE(CALL_0)
OP_CODE(CALL_1)
OP_CODE(CALL_2)
OP_CODE(CALL_3)
OP_CODE(CALL_4)
OP_CODE(CALL_5)
OP_CODE(CALL_6)
OP_CODE(CALL_7)
OP_CODE(CALL_8)
OP_CODE(CALL_9)
OP_CODE(CALL_10)
OP_CODE(CALL_11)
OP_CODE(CALL_12)
OP_CODE(CALL_13)
OP_CODE(CALL_14)
OP_CODE(CALL_15)
OP_CODE(CALL_16)

OP_CODE(OBJECT_RECORD)
OP_CODE(OBJECT_RECORD_DEF)

OP_CODE(OBJECT_ARRAY)

OP_CODE(GET_INDEX)
OP_CODE(SET_INDEX)

OP_CODE(CLOSE_UPVALUE)

/*
  -------------- Single Short Arguments -------------
*/
OP_CODE(POP_JUMP_IF_FALSE)
OP_CODE(POP_JUMP_IF_TRUE)

OP_CODE(JUMP_IF_FALSE)
OP_CODE(JUMP_IF_TRUE)

OP_CODE(JUMP)
OP_CODE(LOOP)

/*
  -------------- Double Byte Arguments -------------
*/
OP_CODE(VARCALL)

OP_CODE(GET_PROPERTY)
OP_CODE(SET_PROPERTY)

/*
  -------------- Variable-length Arguments -------------
*/
OP_CODE(CLOSURE)
