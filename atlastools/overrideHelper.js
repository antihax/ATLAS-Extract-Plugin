// Check for broken or missing nodes

"use strict";
const helpers = require("./include/helpers.js");

const fs = require("fs");

let data = JSON.parse(fs.readFileSync("json/assettypes.json"));
let resourceTypes = JSON.parse(fs.readFileSync("./resourceTypes.json"));

let assets = new Set();

for (let key in data) {
	for (let resource in data[key]) {
		if (resource.includes("Seaweed") || resource.includes("Algae") || key.includes("Algae") || key.includes("Seaweed")) {
			assets.add(key);
		}
	}
}

let types = ["Seaweed"];
let overrides = [];
types.forEach((type) => {
	let override = {
		Key: `${type}-antihax`,
		FoliageMap: {}
	};

	assets.forEach((asset) => {
		override.FoliageMap[asset] = `/Game/Atlas/AtlasCoreBP/HarvestComponents/00_OceanFloor/FiberHarvestComponent_Seaweed.FiberHarvestComponent_Seaweed`;
	});
	overrides.push(override);
});

fs.writeFileSync("./json/overrides-temp.json", JSON.stringify(overrides, null, "\t"));
