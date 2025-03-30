#ifndef GAUSSIAN_SPLATTING_UTILS_H
#define GAUSSIAN_SPLATTING_UTILS_H

#include <android/log.h>
#include <vector>
#include <android/asset_manager_jni.h>


// LOGO always prints
#define LOGO(...) __android_log_print(ANDROID_LOG_INFO, "3dgs_cpp_root_output", __VA_ARGS__)

// LOGD only prints in debug mode
#ifdef DEBUG
    #define LOGD(...) __android_log_print(ANDROID_LOG_INFO, "3dgs_cpp_root_debug", __VA_ARGS__)
#else
    #define LOGD(...) ((void)0)  // Expands to nothing, optimizing out the calls
#endif

enum ProfilingMode {
    NONE,
    FPS,
    PSNR,
    MEM,
};

namespace cnpy {

    struct NpyArray {
        NpyArray(const std::vector<size_t>& _shape, size_t _word_size, bool _fortran_order) :
                shape(_shape), word_size(_word_size), fortran_order(_fortran_order)
        {
            num_vals = 1;
            for(size_t i = 0;i < shape.size();i++) num_vals *= shape[i];
            data_holder = std::shared_ptr<std::vector<char>>(
                    new std::vector<char>(num_vals * word_size));
        }

        NpyArray() : shape(0), word_size(0), fortran_order(0), num_vals(0) { }

        template<typename T>
        T* data() {
            return reinterpret_cast<T*>(&(*data_holder)[0]);
        }

        template<typename T>
        const T* data() const {
            return reinterpret_cast<T*>(&(*data_holder)[0]);
        }

        template<typename T>
        std::vector<T> as_vec() const {
            const T* p = data<T>();
            return std::vector<T>(p, p+num_vals);
        }

        size_t num_bytes() const {
            return data_holder->size();
        }

        std::shared_ptr<std::vector<char>> data_holder;
        std::vector<size_t> shape;
        size_t word_size;
        bool fortran_order;
        size_t num_vals;
    };

    void parse_npy_header(AAsset* asset, size_t& word_size, std::vector<size_t>& shape, bool& fortran_order);
    NpyArray npy_load(AAssetManager *assetManager, const char * fname);

    template<typename T> std::vector<char>& operator+=(std::vector<char>& lhs, const T rhs) {
        //write in little endian
        for(size_t byte = 0; byte < sizeof(T); byte++) {
            char val = *((char*)&rhs+byte);
            lhs.push_back(val);
        }
        return lhs;
    }

    template<> std::vector<char>& operator+=(std::vector<char>& lhs, const std::string rhs);
    template<> std::vector<char>& operator+=(std::vector<char>& lhs, const char* rhs);

    void
    parse_npy_header(AAsset *asset, size_t &word_size, std::vector<size_t> &shape,
                     bool &fortran_order);

    void
    parse_npy_header(AAsset *asset, size_t &word_size, std::vector<size_t> &shape,
                     bool &fortran_order);
}

#endif //GAUSSIAN_SPLATTING_UTILS_H
