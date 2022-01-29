// Check for broken or missing nodes

'use strict';
const helpers = require('./include/helpers.js');

const fs = require('fs');
const whloc = [437, -337];
const calibration = 200 / 1400000 / 11;
const whdistance = calibration * 200000.0;
const farmDistance = calibration * 5000.0;
const farmradius = calibration * 2400.0;

let rawdata = fs.readFileSync('DumpResources/server/ShooterGame/Binaries/Win64/resources/K9.json');
let data = JSON.parse(rawdata);

rawdata = fs.readFileSync('./resourceTypes.json');
let resourceTypes = JSON.parse(rawdata);

let harvestable = {};
let harvestableUnique = {};

for (let island in data.DetailedResources) {
    for (let resource in data.DetailedResources[island]) {
        let resources = resource.replace(/ \(Rock\)/g, "").split(", ");
        Array.from(data.DetailedResources[island][resource]).forEach(coords => {
            // coords = helpers.translateGPS(coords);

            if (helpers.distance(whloc, coords) > whdistance)
                return;

            resources.forEach(type => {

                harvestableUnique[resourceTypes[type] + " " + type] = 1;

                if (!harvestable[resourceTypes[type]])
                    harvestable[resourceTypes[type]] = new Array();
                harvestable[resourceTypes[type]].push({
                    gps: coords,
                    resource: type,
                    resources: resources
                });
            });
        });
    }
}

console.log(harvestableUnique);

helpers.stop();
let ignore = [];
let pairs = [];
harvestable.Stone.forEach(node1 => {

    if (node1.resource == "Sandstone")
        console.log(node1.gps, helpers.translateGPS(node1.gps), node1.resource);


    harvestable.Metal.forEach(node2 => {
        // if (node1.resource != node2.resource) {
        if (node1.resource == "Copper") {
            for (let i in ignore) {
                if (helpers.distance(ignore[i], node2.gps) < farmDistance)
                    return;
            }

            if (helpers.distance(node1.gps, node2.gps) < farmradius) {
                ignore.push(node1.gps);
                console.log(node1.gps, node1.resource, node2.resource);
                pairs.push({
                    node1: node1,
                    node2: node2
                })
                return;
            }
        }
    })
})

fs.writeFileSync('./json/WHOutput.json', JSON.stringify(pairs, null, "\t"));
fs.writeFileSync('./json/WHOutputAll.json', JSON.stringify(harvestable, null, "\t"));