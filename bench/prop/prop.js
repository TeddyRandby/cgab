let obj = {
    zebra: 0,
    camel: 0,
    cow: 0,
    rooster: 0,
}

for (let i = 0; i < 500000; i++) {
    obj.zebra = i
    obj.camel = obj.zebra - 1
    obj.cow = obj.camel + 1
    obj.rooster = obj.cow - obj.zebra
}
