// Check for broken or missing nodes

'use strict';
const fs = require('fs');

let rawdata = fs.readFileSync('./json/islandExtended.json');
let islands = JSON.parse(rawdata);
rawdata = fs.readFileSync('./resourceTypes.json');
let resourceTypes = JSON.parse(rawdata);
let types = {};
let resources = {};

let foundAsset = {};

for (let resource in resourceTypes) {
	resources[resource] = {
		count: 0,
		type: resourceTypes[resource],
	};
}

for (let island in islands) {
	let i = islands[island];
	for (let asset in i.assets) {
		if (!foundAsset[asset]) foundAsset[asset] = 1;
		else foundAsset[asset]++;
	}
	for (let resource in i.resources) {
		resource = resource.replace(/ \(Rock\)/g, '');
		if (!resources[resource].type) {
			console.log('Unknown ' + resource);
		}
		resources[resource].count++;
	}
}

for (let type in resourceTypes) {
	types[resourceTypes[type]] = [];
}

for (let type in types) {
	for (let resource in resources) {
		if (resources[resource].type === type) {
			let add = {};
			add[resource] = resources[resource].count;
			types[type].push(add);
		}
	}
}

fs.writeFileSync('./json/resourceCheck.json', JSON.stringify(types, null, '\t'));
fs.writeFileSync('./json/assetCheck.json', JSON.stringify(foundAsset, null, '\t'));
