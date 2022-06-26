// Check for broken or missing nodes

"use strict";
const helpers = require("./include/helpers.js");

const fs = require("fs");
const whloc = [391, -190];
const calibration = 200 / 1400000 / 13;
const whdistance = calibration * 4500000.0;

let rawdata = fs.readFileSync("DumpResources/server/ShooterGame/Binaries/Win64/resources/H10.json");
let data = JSON.parse(rawdata);
let url = "";

for (let key in data.Animals) {
	for (let a in data.Animals[key]) {
		let animal = data.Animals[key][a];
		for (let l in animal.animals) {
			let list = animal.animals[l];
			for (let a in list) {
				if (!animal.gps) continue;
				//			if (helpers.distance(whloc, animal.gps[0]) > whdistance) continue;
				if (helpers.includes(list, "Bear")) {
					for (let a in list) {
						let an = list[a];
						for (let g in animal.gps) {
							let gps = animal.gps[g];
							url += helpers.GPStoLeaflet(gps[0], gps[1]).join(";") + ",";
							console.log(animal.spawnLimits);
						}
					}
				}
			}
		}
	}
}
console.log("http://atlas.antihax.net/#" + url.slice(0, -1));
helpers.stop();
