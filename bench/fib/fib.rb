def Fib(i)
  if i < 2
    return i
  end
  return Fib(i - 1) + Fib(i - 2)
end

Fib(30)
