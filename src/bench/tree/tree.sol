let bottom_up_tree = fn(item, depth) then
    if not (depth == 0) then
        let i = item + item
        let left = bottom_up_tree(i-1, depth - 1)
        let right = bottom_up_tree(i, depth - 1)
        return [item, left, right]
    end else 
        return [item]
    end
end

let item_check = fn(tree) then
    if tree[1] then
        return tree[0] + item_check(tree[1]) - item_check(tree[2])
    end else
        return tree[0]
    end
end

let pow_two = fn(n) =>
    if n == 0 =>
        1
    else
        pow_two(n-1) * 2

let run = fn(N) then
  let mindepth = 4
  let maxdepth = mindepth + 2

  if maxdepth < N then maxdepth = N

  let stretchdepth = maxdepth + 1
  let tree = bottom_up_tree(0, stretchdepth)

  item_check(tree)::log.info()

  let longlivedtree = bottom_up_tree(0, maxdepth)

  let depth = mindepth
  while depth < maxdepth + 1 then do
      let iters = pow_two(maxdepth - depth + mindepth)
      let check = 0
      let checks = 0

      while check < iters then do
          let tree1 = bottom_up_tree(1, depth)

          #item_check(tree1):std.log.info()

          checks = checks + item_check(tree1) 

          let tree2 = bottom_up_tree(-1, depth)

          checks = checks + item_check(tree2)

          check = check + 1
      end

      :log.info(checks)

      depth = depth + 2
  end

  :log.info(item_check(longlivedtree))
end

run(13)
