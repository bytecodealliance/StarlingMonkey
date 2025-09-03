import js from "@eslint/js";
import tseslint from "typescript-eslint";
import { defineConfig, globalIgnores } from "eslint/config";

export default tseslint.config(
    globalIgnores(["src/sourcemaps/*.ts", "**/*.d.ts", "tests/**/*", "dist/**/*", "out/**/*", "starling-debugger/**/*", "eslint.config.mjs"]),
    js.configs.recommended,
    tseslint.configs.recommended,
    {
        rules: {
            // "@typescript-eslint/no-explicit-any": "off",
            "@typescript-eslint/naming-convention": "warn",
            "@typescript-eslint/prefer-as-const": "warn",
            "@typescript-eslint/no-unused-vars": [
                "warn",
                { argsIgnorePattern: "^_", varsIgnorePattern: "^_", caughtErrorsIgnorePattern: "^_" }
            ],
            "no-unused-vars": "off",
            "prefer-const": "warn",
            curly: "warn",
            eqeqeq: "warn",
            "@typescript-eslint/no-explicit-any": 'warn',
            "no-throw-literal": "warn",
            semi: "error",
        },
    }
);
