vscode = require('vscode');

hello = require('./runtime/tests/hello/extension.js');
hello.activate(vscode.ExtensionContext);
vscode.commands.commands['hello.helloWorld'].call();

// console.log(JSON.stringify(vscode));
