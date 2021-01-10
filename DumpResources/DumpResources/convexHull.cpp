#include "convexHull.h"
#include <algorithm>
#include <cmath>

// returns true if the three points make a counter-clockwise turn
bool ccw(const point& a, const point& b, const point& c) {
	return ((std::get<0>(b) - std::get<0>(a)) * (std::get<1>(c) - std::get<1>(a)))
			> ((std::get<1>(b) - std::get<1>(a)) * (std::get<0>(c) - std::get<0>(a)));
}

// convexHull finds the outlying points in set of points
std::vector<point> convexHull(std::vector<point> p) {
	if (p.size() == 0) return std::vector<point>();
	std::sort(p.begin(), p.end(), [](point& a, point& b) {
		if (std::get<0>(a) < std::get<0>(b)) return true;
		return false;
		});

	std::vector<point> h;

	// lower hull
	for (const auto& pt : p) {
		while (h.size() >= 2 && !ccw(h.at(h.size() - 2), h.at(h.size() - 1), pt)) {
			h.pop_back();
		}
		h.push_back(pt);
	}

	// upper hull
	auto t = h.size() + 1;
	for (auto it = p.crbegin(); it != p.crend(); it = std::next(it)) {
		auto pt = *it;
		while (h.size() >= t && !ccw(h.at(h.size() - 2), h.at(h.size() - 1), pt)) {
			h.pop_back();
		}
		h.push_back(pt);
	}

	h.pop_back();
	return h;
}