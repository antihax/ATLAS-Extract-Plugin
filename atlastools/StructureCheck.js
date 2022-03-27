// Check for broken or missing nodes

'use strict';

const fs = require('fs');

let rawdata = fs.readFileSync(
	'DumpResources/server/ShooterGame/Binaries/Win64/resources/structures.json',
);
let data = JSON.parse(rawdata);

for (let key in data.Structures) {
	let struc = data.Structures[key];
	if (struc.IsFloor && struc.IsFoundation && struc.ForcePlacingOnGround) {
		console.log(
			key,
			struc.ForceGroundForFoundation,
			struc.MaximumFoundationSupport2DBuildDistance,
			struc.AdditionalFoundationSupportDistanceForLinkedStructures,
		);
	}
}

//helpers.stop();
