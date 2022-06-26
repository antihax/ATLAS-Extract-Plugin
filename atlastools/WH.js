// Check for broken or missing nodes

"use strict";
const helpers = require("./include/helpers.js");

const fs = require("fs");
const whloc = [387.79, -338.27];
const calibration = 200 / 1400000 / 13;
const whdistance = calibration * 45005500.0;
const farmDistance = calibration * 50300.0;
const farmradius = calibration * 4800.0;

let rawdata = fs.readFileSync("DumpResources/server/ShooterGame/Binaries/Win64/resources/E11.json");
let data = JSON.parse(rawdata);

rawdata = fs.readFileSync("./resourceTypes.json");
let resourceTypes = JSON.parse(rawdata);

let harvestable = {};
let harvestableUnique = {};

for (let island in data.DetailedResources) {
	for (let resource in data.DetailedResources[island]) {
		let resources = resource.replace(/ \(Rock\)/g, "").split(", ");

		Array.from(data.DetailedResources[island][resource]).forEach((coords) => {
			// coords = helpers.translateGPS(coords);

			if (helpers.distance(whloc, coords) > whdistance) return;
			resources.forEach((type) => {
				harvestableUnique[resourceTypes[type] + " " + type] = 1;

				if (!harvestable[resourceTypes[type]]) harvestable[resourceTypes[type]] = new Array();
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
let url = "";
harvestable.Vegetable.forEach((node1) => {
	harvestable.Thatch.forEach((node2) => {
		if (node1.resource == "Wild Potato")
			if (node1.resource != node2.resource) {
				for (let i in ignore) {
					if (helpers.distance(ignore[i], node2.gps) < farmDistance) return;
				}

				if (helpers.distance(node1.gps, node2.gps) < farmradius) {
					ignore.push(node1.gps);
					url += helpers.GPStoLeaflet(node1.gps[0], node1.gps[1]).join(";") + ",";
					console.log(helpers.translateGPS(node1.gps), node1.resource, node2.resource);
					pairs.push({
						node1: node1,
						node2: node2
					});
					return;
				}
			}
	});
});

console.log("http://atlas.antihax.net/#" + url.slice(0, -1));
fs.writeFileSync("./json/WHOutput.json", JSON.stringify(pairs, null, "\t"));
fs.writeFileSync("./json/WHOutputAll.json", JSON.stringify(harvestable, null, "\t"));
