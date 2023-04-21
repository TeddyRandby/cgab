// Compile-time errors
STATUS(OK, "OK")
STATUS(PANIC, "A fatal error occurred")
STATUS(MALFORMED_STRING, "Unexpected character in string literal")
STATUS(MALFORMED_TOKEN, "Unrecognized token")
STATUS(UNEXPECTED_TOKEN, "Unexpected token")
STATUS(CAPTURED_MUTABLE, "Blocks cannot capture mutable variables")
STATUS(INVALID_IMPLICIT, "Cannot implicitly add parameters after locals")
STATUS(TOO_MANY_LOCALS, "Blocks cannot have more than 255 locals")
STATUS(TOO_MANY_UPVALUES, "Blocks cannot capture more than 255 locals")
STATUS(TOO_MANY_PARAMETERS, "Blocks cannot have more than 16 parameters")
STATUS(TOO_MANY_ARGUMENTS, "Blocks calls cannot have more than 16 arguments")
STATUS(TOO_MANY_RETURN_VALUES, "Blocks cannot return more than 16 values")
STATUS(TOO_MANY_EXPRESSIONS, "Expected fewer expressions")
STATUS(TOO_MANY_EXPRESSIONS_IN_INITIALIZER,
       "Record literals cannot initialize more than 255 properties")
STATUS(TOO_MANY_EXPRESSIONS_IN_LET,
       "'let' expressions cannot declare more than 16 locals")
STATUS(REFERENCE_BEFORE_INITIALIZE,
       "Variables cannot be referenced before they are initialized")
STATUS(LOCAL_ALREADY_EXISTS, "A local with this name already exists")
STATUS(EXPRESSION_NOT_ASSIGNABLE, "The expression is not assignable")
STATUS(MISSING_END, "Block need a corresponding 'end'")
STATUS(MISSING_INITIALIZER, "Variables must be initialized")
STATUS(MISSING_IDENTIFIER, "Identifier could not be resolved")
STATUS(MISSING_RECEIVER, "A message definition should specify a receiver")

// Run-time errors
STATUS(NOT_NUMERIC, "Expected a number")
STATUS(NOT_RECORD, "Expected a record")
STATUS(NOT_STRING, "Expected a string")
STATUS(NOT_CALLABLE, "Expected a callable value")
STATUS(WRONG_ARITY,
       "Could not call a function with the wrong number of arguments")
STATUS(ASSERTION_FAILED, "A runtime assertion failed")
STATUS(OVERFLOW, "Reached maximum call depth")
STATUS(MISSING_PROPERTY, "Property does not exist")
STATUS(IMPLEMENTATION_EXISTS, "Implementation already exists")
STATUS(IMPLEMENTATION_MISSING, "Implementation does not exist")
