
function print_object(obj, depth, arr) {
    if (depth > 8) return; // circular reference?

    let indent = '';
    for(let i=0; i<depth; i++) indent += '  ';

    Object.keys(obj).forEach((k) => {
        if (typeof(obj[k]) === 'object') {
            if (obj[k].length === undefined) {
                app.log(`${indent}${k}: {`);
                print_object(obj[k], depth + 1);
                app.log(`${indent}}`);
            } else {
                app.log(`${indent}${k}: [`);
                obj[k].forEach(c => {
                    if (typeof(c) == 'string') {
                        app.log(`${indent}  ${c}`);
                    } else {
                        print_object(c, depth + 1);  
                    }
                })
                app.log(`${indent}]`);
            }
            return;
        }
        if (typeof(obj[k]) === 'function') {
            app.log(`${indent}${k}: function`);
            return;
        }
        app.log(`${indent}${k}:${obj[k]}`);
    });
}

app.print = (msg) => {
    if (typeof(msg) === 'object') {
        app.log('{');
        print_object(msg, 1);
        app.log('}');
    } else {
        app.log(msg);
    }
}

console = {
    log: app.log
}