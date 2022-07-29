function Fib(n)
  if (n < 2) then return n end
  return Fib(n - 1) + Fib(n - 2)
end

print(Fib(28))
