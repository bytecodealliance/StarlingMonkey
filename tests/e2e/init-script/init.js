const builtinMod = {
    func() {
        return 'foo';
    }
}

defineBuiltinModule('builtinMod', builtinMod);
print("initialization done");
