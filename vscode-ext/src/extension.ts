import * as vscode from 'vscode';
import * as path from 'path';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
} from 'vscode-languageclient/node';

let client: LanguageClient | undefined;
let statusBarItem: vscode.StatusBarItem | undefined;

/** Last project info received per URI, used by the picker. */
let lastProjectInfo: ProjectInfoParams | undefined;

interface ProjectEntry {
    file: string;
    chip: string;
    name: string;
}

interface ProjectInfoParams {
    uri: string;
    projects: ProjectEntry[];
    activeIndex: number;
}

function getServerCommand(): string {
    const config = vscode.workspace.getConfiguration('jz-hdl');
    const binaryPath = config.get<string>('binaryPath', '');
    return binaryPath || 'jz-hdl';
}

function isLspEnabled(): boolean {
    const config = vscode.workspace.getConfiguration('jz-hdl');
    return config.get<boolean>('lsp.enabled', true);
}

function updateStatusBar(params: ProjectInfoParams): void {
    if (!statusBarItem) return;

    // Store for the picker command.
    lastProjectInfo = params;

    if (params.projects.length === 0) {
        statusBarItem.text = "$(circuit-board) No Project";
        statusBarItem.tooltip = "No JZ-HDL project file found";
        statusBarItem.command = undefined;
        statusBarItem.show();
        return;
    }

    if (params.projects.length === 1 &&
        (params.activeIndex < 0 || params.activeIndex >= params.projects.length)) {
        // One project exists but doesn't import this file.
        statusBarItem.text = "$(circuit-board) No Project";
        statusBarItem.tooltip = "Project found but does not import this file";
        statusBarItem.command = 'jz-hdl.selectProject';
        statusBarItem.show();
        return;
    }

    if (params.activeIndex < 0 || params.activeIndex >= params.projects.length) {
        statusBarItem.text = "$(circuit-board) No Project";
        statusBarItem.tooltip = `${params.projects.length} project(s) found but none imports this file\nClick to select`;
        statusBarItem.command = 'jz-hdl.selectProject';
        statusBarItem.show();
        return;
    }

    const active = params.projects[params.activeIndex];
    const name = active.name !== '-' ? active.name : 'Unknown';
    const chip = active.chip !== '-' ? active.chip : '';

    statusBarItem.text = chip
        ? `$(circuit-board) ${name} [${chip}]`
        : `$(circuit-board) ${name}`;

    // Build tooltip with all projects.
    const lines: string[] = [];
    for (let i = 0; i < params.projects.length; i++) {
        const p = params.projects[i];
        const pName = p.name !== '-' ? p.name : '?';
        const pChip = p.chip !== '-' ? p.chip : 'no chip';
        const marker = i === params.activeIndex ? ' (active)' : '';
        const basename = path.basename(p.file);
        lines.push(`${pName} [${pChip}] - ${basename}${marker}`);
    }
    if (params.projects.length > 1) {
        lines.push('', 'Click to switch project');
    }
    statusBarItem.tooltip = lines.join('\n');
    statusBarItem.command = params.projects.length > 1 ? 'jz-hdl.selectProject' : undefined;
    statusBarItem.show();
}

async function showProjectPicker(): Promise<void> {
    if (!lastProjectInfo || lastProjectInfo.projects.length === 0) {
        vscode.window.showInformationMessage('No JZ-HDL projects discovered.');
        return;
    }

    const items = lastProjectInfo.projects.map((p, i) => {
        const name = p.name !== '-' ? p.name : '?';
        const chip = p.chip !== '-' ? p.chip : 'no chip';
        const basename = path.basename(p.file);
        const active = i === lastProjectInfo!.activeIndex ? ' $(check)' : '';
        return {
            label: `${name}${active}`,
            description: `[${chip}]`,
            detail: basename,
            index: i,
            projectFile: p.file,
        };
    });

    const picked = await vscode.window.showQuickPick(items, {
        placeHolder: 'Select a JZ-HDL project for this file',
    });

    if (!picked || !client) return;

    // Send the selection to the LSP server.
    client.sendNotification('jz-hdl/selectProject', {
        uri: lastProjectInfo.uri,
        projectFile: picked.projectFile,
    });
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

    const config = vscode.workspace.getConfiguration('jz-hdl');

    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'jz-hdl' }],
        initializationOptions: {
            hover: {
                clocks: config.get<boolean>('hover.clocks', true),
                declarations: config.get<boolean>('hover.declarations', true),
            },
        },
    };

    client = new LanguageClient(
        'jz-hdl',
        'JZ-HDL Language Server',
        serverOptions,
        clientOptions
    );

    try {
        await client.start();

        // Listen for project info notifications from the server.
        client.onNotification('jz-hdl/projectInfo', (params: ProjectInfoParams) => {
            updateStatusBar(params);
        });
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
    // Register the project picker command.
    context.subscriptions.push(
        vscode.commands.registerCommand('jz-hdl.selectProject', showProjectPicker)
    );

    // Create the status bar item.
    statusBarItem = vscode.window.createStatusBarItem(
        vscode.StatusBarAlignment.Left, 50
    );
    statusBarItem.name = 'JZ-HDL Project';
    context.subscriptions.push(statusBarItem);

    // Show/hide based on active editor language.
    context.subscriptions.push(
        vscode.window.onDidChangeActiveTextEditor((editor) => {
            if (editor && editor.document.languageId === 'jz-hdl') {
                statusBarItem?.show();
            } else {
                statusBarItem?.hide();
            }
        })
    );

    // Show for the initial editor if it's a .jz file.
    if (vscode.window.activeTextEditor?.document.languageId === 'jz-hdl') {
        statusBarItem.text = "$(circuit-board) ...";
        statusBarItem.show();
    }

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
