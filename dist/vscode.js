"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.window = exports.commands = exports.Disposable = exports.ExtensionContext = void 0;
exports.ExtensionContext = {
    subscriptions: [] = []
};
class Disposable {
    constructor(callOnDispose) {
        this.dispose = callOnDispose;
    }
    dispose() {
        return null;
    }
}
exports.Disposable = Disposable;
var commands;
(function (commands_1) {
    commands_1.commands = {};
    function registerCommand(command, callback, thisArg) {
        commands_1.commands[command] = callback;
        return new Disposable(() => {
            delete commands_1.commands[command];
        });
    }
    commands_1.registerCommand = registerCommand;
})(commands = exports.commands || (exports.commands = {}));
var window;
(function (window) {
    function showInformationMessage(message, ...items) {
        let p = new Promise((resolve, reject) => {
            // console.log(message);
            app.log(message);
            resolve(undefined);
        });
        return p;
    }
    window.showInformationMessage = showInformationMessage;
})(window = exports.window || (exports.window = {}));
