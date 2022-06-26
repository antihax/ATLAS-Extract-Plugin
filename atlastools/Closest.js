// Check for broken or missing nodes

'use strict';
const helpers = require('./include/helpers.js');

const fs = require('fs');

let rawdata = fs.readFileSync('DumpResources/server/ShooterGame/Binaries/Win64/resources/B2.json');
let data = JSON.parse(rawdata);

rawdata = fs.readFileSync('./resourceTypes.json');
let resourceTypes = JSON.parse(rawdata);
let closest = [];
for (let island in data.DetailedResources) {
	for (let resource in data.DetailedResources[island]) {
		let resources = resource.replace(/ \(Rock\)/g, '').split(', ');
		Array.from(data.DetailedResources[island][resource]).forEach((coords) => {
			resources.forEach((type) => {
				if (type === 'Cobalt') {
					closest.push(coords);
					console.log(coords);
				}
			});
		});
	}
}

fs.writeFileSync('./json/Closest.json', JSON.stringify(closest, null, '\t'));
