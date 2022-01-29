// Check for broken or missing nodes

'use strict';
const helpers = require('./include/helpers.js');

const fs = require('fs');
const whloc = [17.97, -4.38];
const calibration = 200 / 1400000 / 11;
const whdistance = calibration * 45000.0;
const farmDistance = calibration * 5000.0;
const farmradius = calibration * 2400.0;

let rawdata = fs.readFileSync('DumpResources/server/ShooterGame/Binaries/Win64/resources/G6.json');
let data = JSON.parse(rawdata);

rawdata = fs.readFileSync('./resourceTypes.json');
let resourceTypes = JSON.parse(rawdata);

let harvestable = {};

for (let key in data.Animals) {
    for (let a in data.Animals[key]) {
        let animal = data.Animals[key][a];
        for (let list in animal.animals) {
            if (!animal.gps)
                continue;

           if (helpers.distance(whloc, animal.gps[0]) > whdistance)
                continue;

            if (animal.animals[list].name == "Cow" ) {
                console.log(animal.animals[list], animal.spawnLimits, animal.gps[0]);
                continue;
            }
        }
    }
}

helpers.stop();