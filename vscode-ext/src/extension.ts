import * as vscode from 'vscode';
import * as path from 'path';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
} from 'vscode-languageclient/node';

let client: LanguageClient | undefined;

function getServerCommand(): string {
    const config = vscode.workspace.getConfiguration('jz-hdl');
    const binaryPath = config.get<string>('binaryPath', '');
    return binaryPath || 'jz-hdl';
}

function isLspEnabled(): boolean {
    const config = vscode.workspace.getConfiguration('jz-hdl');
    return config.get<boolean>('lsp.enabled', true);
}

async function startClient(): Promise<void> {
    if (client) {
        return;
    }

    const command = getServerCommand();

    const serverOptions: ServerOptions = {
        command: command,
        args: ['--lsp'],
    };

    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'jz-hdl' }],
    };

    client = new LanguageClient(
        'jz-hdl',
        'JZ-HDL Language Server',
        serverOptions,
        clientOptions
    );

    try {
        await client.start();
    } catch (err) {
        const message = err instanceof Error ? err.message : String(err);
        vscode.window.showErrorMessage(
            `Failed to start JZ-HDL language server: ${message}. ` +
            `Check that the jz-hdl binary is installed and accessible.`
        );
        client = undefined;
    }
}

async function stopClient(): Promise<void> {
    if (client) {
        await client.stop();
        client = undefined;
    }
}

export async function activate(context: vscode.ExtensionContext): Promise<void> {
    if (isLspEnabled()) {
        await startClient();
    }

    // React to configuration changes.
    context.subscriptions.push(
        vscode.workspace.onDidChangeConfiguration(async (e) => {
            if (e.affectsConfiguration('jz-hdl.lsp.enabled') ||
                e.affectsConfiguration('jz-hdl.binaryPath')) {

                if (isLspEnabled()) {
                    // Restart with potentially new binary path.
                    await stopClient();
                    await startClient();
                } else {
                    await stopClient();
                }
            }
        })
    );
}

export async function deactivate(): Promise<void> {
    await stopClient();
}
