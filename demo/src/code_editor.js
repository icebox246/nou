import defaultText from './example.u?raw'

/**
 * @type {(element: HTMLTextAreaElement)}
 **/
export function setupCodeEditor(element) {
    element.classList.add("code-editor-container");

    const textarea = document.createElement("textarea");
    textarea.classList.add("code-editor-textarea")
    textarea.spellcheck = false;
    textarea.wrap = "off";
    textarea.value = defaultText;

    textarea.addEventListener("keydown", (e) => {
        if (e.key === "Tab") {
            e.preventDefault();
            const start = textarea.selectionStart;
            const end = textarea.selectionEnd;
            const indent = "  "
            if (!e.shiftKey) {

                textarea.value = textarea.value.substring(0, start)
                    + indent + textarea.value.substring(end);

                textarea.selectionStart = start + indent.length;
                textarea.selectionEnd = start + indent.length;
            } else {
                if (start < indent.length) return;
                textarea.value = textarea.value.substring(0, start - indent.length) + textarea.value.substring(end);
                textarea.selectionStart = start - indent.length;
                textarea.selectionEnd = start - indent.length;
            }
        }
        console.log(e.key)
    });
    element.appendChild(textarea);

    const highlighted = document.createElement("p");
    highlighted.classList.add("code-editor-highlighted");

    function updateHighglighedText() {
        let text = textarea.value;

        text = text.replace(/</g, "&lt");
        text = text.replace(/>/g, "&gt");

        function hi(group) {
            return `<span class="ü${group}">$1</span>`;
        }

        // numbers
        text = text.replace(/(?<!\w)([0-9]+)/g, hi("nu"));

        // keywords
        text = text.replace(/(?<!\w)(export)(?!\w)/g, hi("kw"));
        text = text.replace(/(?<!\w)(extern)(?!\w)/g, hi("kw"));
        text = text.replace(/(?<!\w)(fn)(?!\w)/g, hi("kw"));
        text = text.replace(/(?<!\w)(return)(?!\w)/g, hi("kw"));
        text = text.replace(/(?<!\w)(if)(?!\w)/g, hi("kw"));
        text = text.replace(/(?<!\w)(else)(?!\w)/g, hi("kw"));

        // types
        text = text.replace(/(?<!\w)(i32)(?!\w)/g, hi("ty"));
        text = text.replace(/(?<!\w)(bool)(?!\w)/g, hi("ty"));

        // boolean
        text = text.replace(/(?<!\w)(true)(?!\w)/g, hi("bo"));
        text = text.replace(/(?<!\w)(false)(?!\w)/g, hi("bo"));

        // functions
        text = text.replace(/(?<!\w)([A-Za-z_][A-Za-z0-9_]*)(?=\s*\()/g, hi("fu"));
        text = text.replace(/(?<!\w)([A-Za-z_][A-Za-z0-9_]*)(?=\s*:=\s*<span class="ükw">fn<\/span>)/g, hi("fu"));
        text = text.replace(/(?<=<span class="ükw">export<\/span>\s*)([A-Za-z_][A-Za-z0-9_]*)/g, hi("fu"));

        // operators
        text = text.replace(/(?<!:)(=)(?!"ü)/g, hi("op"));
        text = text.replace(/(\+)/g, hi("op"));
        text = text.replace(/(-)(?!\&gt)/g, hi("op"));
        text = text.replace(/(\*)/g, hi("op"));
        text = text.replace(/(?<!\<|\/)(\/)(?!\/)/g, hi("op"));
        text = text.replace(/(%)/g, hi("op"));
        text = text.replace(/(?<!\w)(and)(?!\w)/g, hi("op"));
        text = text.replace(/(?<!\w)(or)(?!\w)/g, hi("op"));

        // delimiters
        text = text.replace(/(:)(?!=)/g, hi("de"));
        text = text.replace(/({)/g, hi("de"));
        text = text.replace(/(})/g, hi("de"));
        text = text.replace(/(\()/g, hi("de"));
        text = text.replace(/(\))/g, hi("de"));
        text = text.replace(/(,)/g, hi("de"));
        text = text.replace(/(;)/g, hi("de"));
        text = text.replace(/(-\&gt)/g, hi("de"));
        text = text.replace(/(:=)/g, hi("de"));

        // comments
        text = text.replace(/(\/\/.*\n)/g, hi("co"));

        highlighted.innerHTML = text;
        textarea.style.height = `${textarea.scrollHeight}px`;
        textarea.style.width = `${textarea.scrollWidth}px`;
    }

    textarea.addEventListener("input", updateHighglighedText);
    updateHighglighedText();

    element.appendChild(highlighted);

    return {
        getSourceCode() {
            return textarea.value;
        }
    };
}
