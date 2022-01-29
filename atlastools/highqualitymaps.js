// Find high quality map islands

'use strict';
const helpers = require('./include/helpers.js')
const fs = require('fs');

let rawdata = fs.readFileSync('./json/islands.json');
let islands = JSON.parse(rawdata);

for (var island in islands) {
    var i = islands[island];

    for (var m in i["maps"]) {
        var map = i["maps"][m];
        if (map[1] > 16)
            console.log(i.grid, i.name, map, i.resources['Maps'], i.sublevels.filter(function(v) {
                return /Farth/.test(v)
            }), Object.keys(i.resources).length);
    }
}

helpers.stop(); // Hang so we can debug structures