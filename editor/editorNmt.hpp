/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

// heights of the ground from the digital terrain model of GUGiK (NMT, the WCS service of geoportal.gov.pl): squares of the
// EPSG:2180 grid fetched in the background at 1 m, kept on the disk and sampled in the coordinates of the scenery. the
// scenery is placed on the grid by the PUWG 1992 coordinates of its point (0,0,0), as the orthophoto is: easting = east - x,
// northing = north + z. the heights are those of the model, metres over the sea
class editor_nmt
{
  public:
	editor_nmt();
	~editor_nmt();
	editor_nmt(editor_nmt const &) = delete;
	editor_nmt &operator=(editor_nmt const &) = delete;

	// where the point (0,0,0) of the scenery lies in PUWG 1992; the squares fetched stay, only the mapping changes
	void place(double const North, double const East);
	// a scenery at (0,0) isn't placed anywhere
	bool placed() const;
	// the height of the model at the point of the scenery; false where its square isn't in yet or the model has no data
	bool height(double const X, double const Z, double &Height) const;
	// the squares under the rectangle of the scenery (x, z) asked for in the background, the ones nearest its middle first;
	// true if all of them are in already, those the service couldn't give included
	bool request(glm::dvec2 const &Min, glm::dvec2 const &Max);
	// squares asked for and not in yet
	std::size_t waiting() const;
	// what the last fetch said went wrong, empty when it went right
	std::string error() const;
	// the squares dropped from the memory, and those waiting; the files on the disk stay
	void clear();
	// whether this build can fetch from the service at all
	static bool can_download();

	// heights from the sheets of the model downloaded by hand (ESRI ASCII grid, .asc), as the older ones, in PUWG 1992 or
	// PUWG 2000: where they lie they're taken instead of the service, the rest of the squares come from it. Correction, m:
	// added to their heights, as between Kronsztadt 86 and EVRF2007. an empty folder: the service only. the squares in the
	// memory are dropped when it changes, so they're read anew
	void local(std::string const &Folder, double const Correction);
	// what was found in the folder, for the window
	std::string local_status() const;

	// side of a square of the grid, m
	static double constexpr square_size{500.0};

  private:
	using key = std::pair<int, int>; // easting and northing index of the square on the grid
	struct square
	{
		double east{0.0}; // of the outer edge of the first column
		double north{0.0}; // of the outer edge of the first row, the northernmost
		double pixel{1.0};
		int width{0};
		int height{0};
		std::vector<float> heights; // rows from the north, columns from the west
		bool failed{false}; // the service gave nothing usable: the square has no data
	};

	// a sheet of the folder: its grid as the file says, the place of it in PUWG 1992 worked out once
	struct sheet
	{
		std::string file;
		int zone{0}; // 0: PUWG 1992, 5-8: the zone of PUWG 2000
		double east{0.0}, south{0.0}; // the outer corner of its south-west cell, in its own system
		double cell{1.0};
		int columns{0}, rows{0};
		double nodata{-9999.0};
		double low_east{0.0}, low_north{0.0}, high_east{0.0}, high_north{0.0}; // the extent in PUWG 1992
	};

	key key_of(double const East, double const North) const;
	void work();
	std::shared_ptr<square const> fetch(key const &Key, std::string &Error);
	std::shared_ptr<square const> fetch_service(key const &Key, std::string &Error);
	// the square from the sheets of the folder; null where none of them reaches it. Missing: pixels they don't cover
	std::shared_ptr<square> fetch_local(key const &Key, bool &Missing);
	// the heights of a sheet, rows from the north; read once and kept while a few others are
	std::shared_ptr<std::vector<float> const> sheet_heights(sheet const &Sheet);
	void index_sheets();

	mutable std::mutex m_mutex;
	std::condition_variable m_wake;
	std::vector<std::thread> m_workers;
	std::deque<key> m_queue;
	std::set<key> m_queued;
	std::map<key, std::shared_ptr<square const>> m_squares;
	std::string m_coverage; // the name of the model in the service, read from its capabilities once
	std::string m_error;
	double m_north{0.0};
	double m_east{0.0};
	bool m_stop{false};
	// the sheets of the folder
	std::string m_folder;
	double m_correction{0.0};
	std::vector<sheet> m_sheets;
	std::string m_localstatus;
	double m_indexednorth{0.0}, m_indexedeast{0.0}; // the place of the scenery the systems of the sheets were told apart for
	mutable std::mutex m_sheetmutex; // of the heights read
	std::deque<std::pair<std::string, std::shared_ptr<std::vector<float> const>>> m_sheetheights;
};
