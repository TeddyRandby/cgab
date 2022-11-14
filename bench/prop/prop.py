obj = {
    "zebra": 0,
    "camel": 0,
    "cow": 0,
}

for i in range(500000):
    obj["zebra"] = i
    obj["camel"] = obj["zebra"] - 1
    obj["cow"] = obj["camel"] + 1
    obj["rooster"] = obj["cow"] - obj["zebra"]
