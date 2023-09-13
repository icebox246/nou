import './highlight.css'

export function highlightCode(text) {
    function hi(group) {
        return `↓${group}→$1←`;
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
    text = text.replace(/(?<!\w)([A-Za-z_][A-Za-z0-9_]*)(?=\s*:=\s*↓kw→fn←)/g, hi("fu"));
    text = text.replace(/(?<=→export←\s*)([A-Za-z_][A-Za-z0-9_]*)/g, hi("fu"));

    // operators
    text = text.replace(/(?<!:)(=)/g, hi("op"));
    text = text.replace(/(\+)/g, hi("op"));
    text = text.replace(/(-)(?!\>)/g, hi("op"));
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
    text = text.replace(/(->)/g, hi("de"));
    text = text.replace(/(:=)/g, hi("de"));

    // comments
    text = text.replace(/(\/\/.*\n)/g, hi("co"));


    // cleanup
    text = text.replace(/&/g, "&amp");
    text = text.replace(/</g, "&lt");
    text = text.replace(/>/g, "&gt");

    text = text.replace(/↓(\w\w)→/g, '<span class="hi-$1">')
    text = text.replace(/←/g, '</span>')

    return text;
}
