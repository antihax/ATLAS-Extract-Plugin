'use strict';

const fs = require('fs');

let islands = JSON.parse(fs.readFileSync('./json/islandExtended.json'));
let types = JSON.parse(fs.readFileSync('./json/resourceTypes.json'));
let gridList = JSON.parse(fs.readFileSync('./json/gridList.json'));

for (var island in islands) {
	var i = islands[island];
	i.types = {};

	/* if (!contains(i.resources, ["Mineral Oil", "Crude Oil", "Shale Oil", "Naptha"]))
         continue;
     if (!contains(i.resources, ["Silk"]))
         continue;
         if (contains(i.resources, ["Iridium"]))
         continue;        */
	/*if (!contains(i.resources, ["Twigs", "Reeds", "Ironwood", "Iridium", "Cobalt"]))
        continue;
     if (!contains(i.resources, ["Opal", "Diamond", "Gem", "Garnet", "Ruby", "Sunstone", "Emerald"]))
         continue;

     if (!contains(i.resources, ["Sugar", "Sugars", "Gum", "Honey", "Saps", "Sap", "Nectar", "Sugarcane", "Syrup", "Resin"]))
         continue;

     if (!contains(i.resources, ["Mineral Oil", "Crude Oil", "Shale Oil", "Naptha"]))
         continue;

      if (contains(i.resources, ["Flake Salt", "Salt", "Pink Salt", "Iodine", "Rock Salt", "Kala Namak", "Sea Salt"]))
          if (contains(i.resources, ["Iridium", "Tin", "Silver", "Copper", "Cobalt", "Iron"]))
              if (contains(i.resources, ["Mineral Oil", "Crude Oil", "Shale Oil", "Naptha"]))
                  // if (!i.animals.includes("Snake") && !i.animals.includes("GiantSnake"))*/

	if (gridList[i.grid].forceServerRules !== 3)
       continue;

	var totalTypes = 0;
	for (var type in types) {
		if (hasType(i.resources, type))
			if (i.types[types[type]]) {
				i.types[types[type]]++;
			} else {
				i.types[types[type]] = 1;
				totalTypes++;
			}
	}
	var count =
		i.types['Stone'] + i.types['Thatch'] + i.types['Metal'] + i.types['Fiber'] + i.types['Wood'];
	if (count > 14)
		console.log(
			count,
			String(i.id).padStart(5, ' '),
			i.islandPoints,
			i.name.padStart(10, ' '),
			'\t',
			i.grid.padStart(3, ' '),
			i.types['Stone'],
			i.types['Thatch']|| 0,
			i.types['Metal']|| 0,
			i.types['Fiber']|| 0,
			i.types['Wood']|| 0,
			i.types['Gem'] || 0,
			i.types['Crystal'] || 0,
			i.types['Flint'] || 0,
		);
	//console.dir(i.assets)
}

function sleep(ms) {
	return new Promise((resolve) => setTimeout(resolve, ms));
}

async function demo() {
	await sleep(200000000);
	// Sleep in loop
	for (let i = 0; i < 50; i++) {
		if (i === 3) await sleep(200000000);
	}
}

demo();

function contains(list, search) {
	for (var i in search) {
		for (var x in list) {
			if (x == search[i]) {
				return true;
			}
		}
	}
	return false;
}

function hasType(list, type) {
	for (var x in list) {
		if (x.startsWith(type)) {
			return true;
		}
	}
	return false;
}
