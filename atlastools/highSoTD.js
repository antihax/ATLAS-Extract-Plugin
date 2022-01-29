// Check for broken or missing nodes

'use strict';
const helpers = require('./include/helpers.js');

const fs = require('fs');
const whloc = [66.83, 66.9];
const calibration = 200 / 1400000 / 11;
const whdistance = calibration * 45000.0;
const farmDistance = calibration * 5000.0;
const farmradius = calibration * 2400.0;

let rawdata = fs.readFileSync('json/gridList.json');
let data = JSON.parse(rawdata);

for (let grid in data) {
    let g = data[grid];
    if (g.sotd.levelMultiplier >= 1.01)
        console.log("SOTD:", grid, g.sotd.levelMultiplier, g.sotd.spawnLimits)
    if (g.flotsam.qualityMultiplier >= 1.01)
        console.log("FLOT:", grid, g.flotsam.qualityMultiplier, g.flotsam.spawnLimits)
    if (g.sunktreasure && g.sunktreasure.qualityMultiplier > 1)
        console.log("SUNK:", grid, g.sunktreasure.qualityMultiplier, g.sunktreasure.spawnLimits)
}

helpers.stop();