class SimpleMatrix
  attr_accessor :data

  def initialize(n, data)
    @data = data
    @N = n
  end

  def +(other)
    n = @N
    data = Array.new(n * n)

    (n*n).times do |i|
      data[i] = @data[i] + other.data[i]
    end

    return SimpleMatrix.new(n, data)
  end

  def *(other)
    n = @N
    data = Array.new(n * n)

    n.times do |i|
      n.times do |j|
        sum = 0

        n.times do |k|
          sum += @data[i*n+k] * other.data[k*n+j]
        end

        data[i*n+j] = sum
      end
    end
    
    return SimpleMatrix.new(n, data)
  end

  def diag_sum
    n = @N
    sum = 0

    n.times do |i|
      sum += @data[i*(n + 1)]
    end

    return sum
  end
end

sum = 0

n = 2

5000.times do |i|
  a = SimpleMatrix.new(n, Array.new(n * n).map { i | i })
  b = SimpleMatrix.new(n, Array.new(n * n).map { i | i })

  c = a + b
  d = c * a * b

  sum = d.diag_sum
end

print(sum)
