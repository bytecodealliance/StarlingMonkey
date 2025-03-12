const builtinMod = {
    func() {
        return 'foo';
    }
}

defineBuiltinModule('builtinMod', builtinMod);
if (typeof print !== 'undefined')
    print("initialization done");
