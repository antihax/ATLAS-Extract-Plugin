// Check for broken or missing nodes

"use strict";
const helpers = require("./include/helpers.js");

const fs = require("fs");
const calibration = 200 / 1400000 / 13;
const farmDistance = calibration * 500000.0;
let rawdata = fs.readFileSync("./resourceTypes.json");

let ignore = [];
let url = "";

for (let i = 0; i < 13; i++) {
	for (let j = 0; j < 13; j++) {
		rawdata = fs.readFileSync(`DumpResources/server/ShooterGame/Binaries/Win64/resources/${String.fromCharCode(i + 65)}${j + 1}.json`);
		let data = JSON.parse(rawdata);

		for (let island in data.DetailedResources) {
			for (let resource in data.DetailedResources[island]) {
				let resources = resource.split(", ");
				if (resources.includes("Calcite")) {
					Array.from(data.DetailedResources[island][resource]).forEach((coords) => {
						for (let i in ignore) {
							if (helpers.distance(ignore[i], coords) < farmDistance) return;
						}
						ignore.push(coords);

						url += helpers.GPStoLeaflet(coords[0], coords[1]).join(";") + ",";
					});
				}
			}
		}
	}
}

console.log("http://atlas.antihax.net/#" + url.slice(0, -1));
