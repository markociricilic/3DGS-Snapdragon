//Copyright (C) 2011  Carl Rogers
//Released under MIT License
//license available in LICENSE file, or at http://www.opensource.org/licenses/mit-license.php

#include "base_utils.h"
#include <complex>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <stdint.h>
#include <stdexcept>
#include <regex>


template<> std::vector<char>& cnpy::operator+=(std::vector<char>& lhs, const std::string rhs) {
    lhs.insert(lhs.end(),rhs.begin(),rhs.end());
    return lhs;
}

template<> std::vector<char>& cnpy::operator+=(std::vector<char>& lhs, const char* rhs) {
    // write in little endian
    size_t len = strlen(rhs);
    lhs.reserve(len);
    for(size_t byte = 0; byte < len; byte++) {
        lhs.push_back(rhs[byte]);
    }
    return lhs;
}

// Helper function to read a line from AAsset (since fgets doesn't work on AAsset)
std::string AAsset_getline(AAsset* asset) {
    std::string line;
    char c;
    while (AAsset_read(asset, &c, 1) == 1) {
        if (c == '\n') break;
        line += c;
    }
    return line;
}

void cnpy::parse_npy_header(AAsset* asset, size_t& word_size, std::vector<size_t>& shape, bool& fortran_order) {
    char buffer[11];
    size_t res = AAsset_read(asset, buffer, 11);
    if (res != 11)
        throw std::runtime_error("parse_npy_header: failed AAsset_read");

    std::string header = AAsset_getline(asset);  // Read the header line

    size_t loc1, loc2;

    // Find "fortran_order"
    loc1 = header.find("fortran_order");
    if (loc1 == std::string::npos)
        throw std::runtime_error("parse_npy_header: failed to find header keyword: 'fortran_order'");
    loc1 += 16;
    fortran_order = (header.substr(loc1, 4) == "True");

    // Find shape
    loc1 = header.find("(");
    loc2 = header.find(")");
    if (loc1 == std::string::npos || loc2 == std::string::npos)
        throw std::runtime_error("parse_npy_header: failed to find header keyword: '(' or ')'");

    std::regex num_regex("[0-9][0-9]*");
    std::smatch sm;
    shape.clear();

    std::string str_shape = header.substr(loc1 + 1, loc2 - loc1 - 1);
    while (std::regex_search(str_shape, sm, num_regex)) {
        shape.push_back(std::stoi(sm[0].str()));
        str_shape = sm.suffix().str();
    }

    // Parse "descr" field for word size
    loc1 = header.find("descr");
    if (loc1 == std::string::npos)
        throw std::runtime_error("parse_npy_header: failed to find header keyword: 'descr'");
    loc1 += 9;
//    bool littleEndian = (header[loc1] == '<' || header[loc1] == '|');
//    assert(littleEndian);

    std::string str_ws = header.substr(loc1 + 2);
    loc2 = str_ws.find("'");
    word_size = std::stoi(str_ws.substr(0, loc2));
}

cnpy::NpyArray load_the_npy_file(AAsset * asset) {
    std::vector<size_t> shape;
    size_t word_size;
    bool fortran_order;

    cnpy::parse_npy_header(asset, word_size, shape, fortran_order);

    cnpy::NpyArray arr(shape, word_size, fortran_order);

    // Read the actual data
    size_t nread = AAsset_read(asset, arr.data<char>(), arr.num_bytes());
    if(nread != arr.num_bytes())
        throw std::runtime_error("load_the_npy_file: failed AAsset_read");
    return arr;
}

cnpy::NpyArray cnpy::npy_load(AAssetManager *assetManager, const char * fname) {
    AAsset *asset = AAssetManager_open(assetManager, fname, AASSET_MODE_BUFFER);

    if(!asset) throw std::runtime_error("npy_load: Unable to open file " + std::string(fname));

    cnpy::NpyArray arr = load_the_npy_file(asset);

    AAsset_close(asset);
    return arr;
}