
def acc(a, n):
    if n == 0:
        return a
    else:
        return [n, *acc(a, n - 1)]

def main():
    print(len(acc([], 950)))

if (__name__ == "__main__"):
    main()
