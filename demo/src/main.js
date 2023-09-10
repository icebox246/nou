import './style.css'
import { Compiler } from './compiler.js'
import { setupCodeEditor } from './code_editor';
import { setupButton } from './button';
import { setupCollapsibleSection } from './collapsible_section';
import { Runner } from './runner';

async function main() {
    const compiler = new Compiler();

    await compiler.init("u.wasm");

    const editor = setupCodeEditor(document.querySelector('#code-editor'));

    const tokens_section = setupCollapsibleSection(document.querySelector('section#compiler-tokens'), "tokens", true);
    const vis_section = setupCollapsibleSection(document.querySelector('section#compiler-visualization'), "visualization", true);
    const error_section = setupCollapsibleSection(document.querySelector('section#compiler-errors'), "compiler logs");
    const output_section = setupCollapsibleSection(document.querySelector('section#app-output'), "output");

    setupButton(document.querySelector('#run-button'), async () => {
        await compiler.compile(editor.getSourceCode(), ["-t"]);
        tokens_section.setContent(compiler.getOutput());

        const result = await compiler.compile(editor.getSourceCode(), ["-v"]);
        vis_section.setContent(compiler.getOutput());

        error_section.setContent(compiler.getErrors());

        if (result) {
            const runner = new Runner();
            await runner.init(result);
            runner.run();

            output_section.setContent(runner.getOutput());
        } else {
            output_section.setContent("");
        }
    });
}

main();
