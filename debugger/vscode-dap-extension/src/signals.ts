export class Signal<Val, Err> {
    private _promise!: Promise<Val>;
    private _resolve!: (val: Val) => void;
    private _reject!: (err: Err) => void;
    private _handled = false;

    constructor() {
        this.setPromise();
    }

    wait(): Promise<Val> {
        this._handled = true;
        return this._promise;
    }

    resolve(value: Val) {
        this._resolve(value);
        this.setPromise();
    }

    reject(err: Err) {
        this._reject(err);
        this.setPromise();
    }

    get handled() {
        return this._handled;
    }

    private setPromise() {
        this._handled = false;
        this._promise = new Promise((resolve, reject) => {
            this._resolve = resolve;
            this._reject = reject;
        });
    }
}