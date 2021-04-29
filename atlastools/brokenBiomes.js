// Look for islands without biomes

'use strict';
require('tty')
require('./include/helpers.js');
const fs = require('fs');

let rawdata = fs.readFileSync('./json/islandExtended.json');
let islands = JSON.parse(rawdata);

for (var island in islands) {
    var i = islands[island];
    if (!i.name.includes("ControlPoint") && !i.name.includes("Ice_") && !i.name.includes("Trench_"))
        if (!i.biomes || i.biomes.length == 0)
            console.log(i.grid, ",", i.name);
}