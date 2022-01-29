// Check for broken or missing nodes

'use strict';
const helpers = require('./include/helpers.js');
const fs = require('fs');


let rawdata = fs.readFileSync('./json/craftables.json');
let craft = JSON.parse(rawdata);

rawdata = fs.readFileSync('./json/animals.json');
let animals = JSON.parse(rawdata);

let harvestable = {};

for (let key in animals.Animals) {
    let a = animals.Animals[key];
    for (let food in a.food) {
        let f = a.food[food];
        if (f.foodEffectivenessMultiplier > 0 && craft.Foods[f.name] && craft.Foods[f.name].stats.Food) {
            let foodAmount = f.foodEffectivenessMultiplier * craft.Foods[f.name].stats.Food.add;
            let foodWeight = foodAmount / craft.Foods[f.name].Weight;
            console.log(key, f.name, foodAmount, foodWeight, craft.Foods[f.name].SpoilTime)
        }
    }
}

helpers.stop();