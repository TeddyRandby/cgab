// Compile-time errors
STATUS(OK, "OK")
STATUS(MALFORMED_TOKEN, "Unrecognized token")
STATUS(UNEXPECTED_TOKEN, "Unexpected token")
STATUS(TOO_MANY_LOCALS, "Functions cannot have more than 255 locals")
STATUS(TOO_MANY_UPVALUES, "Functions cannot capture more than 255 locals")
STATUS(TOO_MANY_PARAMETERS, "Functions cannot have more than 16 parameters")
STATUS(TOO_MANY_ARGUMENTS, "Function calls cannot have more than 16 arguments")
STATUS(TOO_MANY_RETURN_VALUES, "Functions cannot return more than 16 values")
STATUS(TOO_MANY_EXPRESSIONS, "Expected fewer expressions")
STATUS(TOO_MANY_EXPRESSIONS_IN_INITIALIZER,
      "'{}' expressions cannot initialize more than 255 properties")
STATUS(TOO_MANY_EXPRESSIONS_IN_LET, "'let' expressions cannot declare more than 16 locals")
STATUS(REFERENCE_BEFORE_INITIALIZE,
      "Variables cannot be referenced before they are initialized")
STATUS(LOCAL_ALREADY_EXISTS, "A local with this name already exists")
STATUS(EXPRESSION_NOT_ASSIGNABLE, "The expression is not assignable")
STATUS(MISSING_END, "'do' expressions need a corresponding 'end'")
STATUS(MISSING_INITIALIZER, "Variables must be initialized")
STATUS(MISSING_IDENTIFIER, "Identifier could not be resolved")

// Run-time errors
STATUS(NOT_NUMERIC, "Could not do numeric operations on a non-number")
STATUS(NOT_OBJECT, "Could not do object operations on a non-object")
STATUS(NOT_STRING, "Could not do string operations on a non-string")
STATUS(NOT_FUNCTION, "Could not do function operations a non-function")
STATUS(WRONG_ARITY, "Could not call a function with the wrong number of arguments")
STATUS(ASSERTION_FAILED, "A runtime assertion failed")
