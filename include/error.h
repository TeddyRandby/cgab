// Compile-time errors
ERROR(MALFORMED_TOKEN, "Unrecognized token")
ERROR(UNEXPECTED_TOKEN, "Unexpected token")
ERROR(TOO_MANY_LOCALS, "Functions cannot have more than 255 locals")
ERROR(TOO_MANY_UPVALUES, "Functions cannot capture more than 255 locals")
ERROR(TOO_MANY_PARAMETERS, "Functions cannot have more than 16 parameters")
ERROR(TOO_MANY_ARGUMENTS, "Function calls cannot have more than 16 arguments")
ERROR(TOO_MANY_RETURN_VALUES, "Functions cannot return more than 16 values")
ERROR(TOO_MANY_EXPRESSIONS, "Tuples cannot have more than 255 values")
ERROR(TOO_MANY_EXPRESSIONS_IN_INITIALIZER,
      "'{}' expressions cannot initialize more than 255 properties")
ERROR(TOO_MANY_EXPRESSIONS_IN_LET, "'let' expressions cannot declare more than 16 locals")
ERROR(REFERENCE_BEFORE_INITIALIZE,
      "Variables cannot be referenced before they are initialized")
ERROR(LOCAL_ALREADY_EXISTS, "A local with this name already exists")
ERROR(EXPRESSION_NOT_ASSIGNABLE, "The expression is not assignable")
ERROR(MISSING_END, "'do' expressions need a corresponding 'end'")
ERROR(MISSING_INITIALIZER, "Variables must be initialized")
ERROR(MISSING_IDENTIFIER, "Identifier could not be resolved")

// Run-time errors
ERROR(NOT_NUMERIC, "Could not do numeric operations on a non-number")
ERROR(NOT_OBJECT, "Could not do object operations on a non-object")
ERROR(NOT_STRING, "Could not do string operations on a non-string")
ERROR(NOT_FUNCTION, "Could not do function operations a non-function")
ERROR(WRONG_ARITY, "Could not call a function with the wrong number of arguments")
ERROR(ASSERTION_FAILED, "A runtime assertion failed")
