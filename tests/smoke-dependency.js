import "./nested-smoke-dependency.js";

console.log("smoke-dependency.js loaded");

export class Foo {
    constructor() {
        console.log("Foo constructor");
    }
}
