// Compile-time errors
STATUS(OK, "ok")
STATUS(NONE, "")
STATUS(PANIC, "A fatal error occurred")
STATUS(MALFORMED_TOKEN, "Unrecognized token")
STATUS(MALFORMED_STRING, "Malformed string literal")
STATUS(MALFORMED_RECORD_KEY, "Malformed key in record literal")
STATUS(MALFORMED_MESSAGE_LITERAL, "Malformed message literal")
STATUS(UNEXPECTED_TOKEN, "Unexpected token")
STATUS(UNEXPECTED_EOF, "Unexpected end of file")
STATUS(CAPTURED_MUTABLE, "Mutable capture")
STATUS(INVALID_BREAK, "Invalid 'break' expression")
STATUS(INVALID_IMPLICIT, "Cannot implicitly add parameters after locals")
STATUS(INVALID_REST_VARIABLE, "Invalid rest variable")
STATUS(TOO_MANY_VARIABLES_IN_DEF, "Cannot define more than 16 variables")
STATUS(TOO_MANY_LOCALS, "Blocks cannot have more than 255 locals")
STATUS(TOO_MANY_UPVALUES, "Blocks cannot capture more than 255 locals")
STATUS(TOO_MANY_PARAMETERS, "Blocks cannot have more than 16 parameters")
STATUS(TOO_MANY_ARGUMENTS, "Blocks calls cannot have more than 16 arguments")
STATUS(TOO_MANY_RETURN_VALUES, "Blocks cannot return more than 16 values")
STATUS(TOO_MANY_EXPRESSIONS, "Expected fewer expressions")
STATUS(TOO_MANY_EXPRESSIONS_IN_INITIALIZER,
       "Record literals cannot initialize more than 255 properties")
STATUS(REFERENCE_BEFORE_INITIALIZE,
       "Uninitialized variable")
STATUS(LOCAL_ALREADY_EXISTS, "A local with this name already exists")
STATUS(EXPRESSION_NOT_ASSIGNABLE,
       "The expression on the left is not assignable")
STATUS(MISSING_END, "Block need a corresponding 'end'")
STATUS(MISSING_INITIALIZER, "Variables must be initialized")
STATUS(MISSING_IDENTIFIER, "Identifier could not be resolved")
STATUS(MISSING_RECEIVER, "A message definition should specify a receiver")

// Run-time errors
STATUS(TYPE_MISMATCH, "Type mismatch")
STATUS(OVERFLOW, "Reached maximum call depth")
STATUS(IMPLEMENTATION_EXISTS, "Implementation already exists")
STATUS(IMPLEMENTATION_MISSING, "Implementation does not exist")
