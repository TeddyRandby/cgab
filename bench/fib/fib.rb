def Fib(i)
  if (i < 2)
    return i
  else
    return Fib(i - 1) + Fib(i - 2)
  end
end

Fib(28)
