import * as vscode from '_vscode'

declare var app: any

export const ExtensionContext : any = {
    subscriptions: [] = []
}

export class Disposable implements vscode.Disposable {
    constructor(callOnDispose: () => any)
    {
        this.dispose = callOnDispose;
    }
    dispose(): any
    {
        return null;
    }
}

export namespace commands {
    export const commands: any = {};
    export function registerCommand(command: string, callback: (...args: any[]) => any, thisArg?: any): vscode.Disposable 
    {
        commands[command] = callback;
        return new Disposable(() => {
            delete commands[command];
        });
    }
}

export namespace window {
    export function showInformationMessage<T extends string>(message: string, ...items: T[]): Thenable<T | undefined>
    {
        let p: Thenable<T | undefined> = new Promise((resolve, reject) => {
            // console.log(message);
            app.log(message);
            resolve(undefined);
        });
        return p;
    }
}