#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace Mark
{
    enum eBitmapFormat
    {
        eBitmapFormat_UnsignedByte,
        eBitmapFormat_Float,
    };
    enum eBitmapType
    {
        eBitmapType_2D,
        eBitmapType_Cube
    };

    static int GetBytesPerComponent(eBitmapFormat _format)
    {
        int BytesPerComponent = 0;

        switch (_format)
        {
        case eBitmapFormat_UnsignedByte:
            BytesPerComponent = 1;
            break;
        case eBitmapFormat_Float:
            BytesPerComponent = 4;
            break;
        };

        return BytesPerComponent;
    }

    struct Bitmap 
    {
        Bitmap() {};

        void Init(int w, int h, int comp, eBitmapFormat fmt)
        {
            m_width = w;
            m_height = h;
            m_components = comp;
            m_format = fmt;
            m_data.resize(w * h * comp * GetBytesPerComponent(fmt));

            initGetSetFuncs();
        }

        Bitmap(int _w, int _h, int _comp, eBitmapFormat _fmt)
            :m_width(_w), m_height(_h), m_components(_comp), m_format(_fmt), m_data(_w* _h* _comp* GetBytesPerComponent(_fmt))
        {
            initGetSetFuncs();
        }

        Bitmap(int _w, int _h, int _d, int _comp, eBitmapFormat _fmt)
            :m_width(_w), m_height(_h), m_depth(_d), m_components(_comp), m_format(_fmt), m_data(_w* _h* _d* _comp* GetBytesPerComponent(_fmt))
        {
            initGetSetFuncs();
        }

        Bitmap(int _w, int _h, int _comp, eBitmapFormat _fmt, void* ptr)
            :m_width(_w), m_height(_h), m_components(_comp), m_format(_fmt), m_data(_w* _h* _comp* GetBytesPerComponent(_fmt))
        {
            initGetSetFuncs();
            memcpy(m_data.data(), ptr, m_data.size());
        }

        int m_width = 0;
        int m_height = 0;
        int m_depth = 1;
        int m_components = 3;
        eBitmapFormat m_format = eBitmapFormat_UnsignedByte;
        eBitmapType m_type = eBitmapType_2D;
        std::vector<uint8_t> m_data;

        void setPixel(int x, int y, glm::vec4& c)
        {
            (*this.*setPixelFunc)(x, y, c);
        }

        glm::vec4 getPixel(int x, int y) const
        {
            return ((*this.*getPixelFunc)(x, y));
        }

    private:

        using setPixel_t = void(Bitmap::*)(int, int, glm::vec4&);
        using getPixel_t = glm::vec4(Bitmap::*)(int, int) const;
        setPixel_t setPixelFunc = &Bitmap::setPixelUnsignedByte;
        getPixel_t getPixelFunc = &Bitmap::getPixelUnsignedByte;

        void initGetSetFuncs()
        {
            switch (m_format)
            {
            case eBitmapFormat_UnsignedByte:
                setPixelFunc = &Bitmap::setPixelUnsignedByte;
                getPixelFunc = &Bitmap::getPixelUnsignedByte;
                break;
            case eBitmapFormat_Float:
                setPixelFunc = &Bitmap::setPixelFloat;
                getPixelFunc = &Bitmap::getPixelFloat;
                break;
            }
        }

        void setPixelFloat(int _x, int _y, glm::vec4& _c)
        {
            int ofs = m_components * (_y * m_width + _x);
            float* data = (float*)(m_data.data());
            if (m_components > 0) data[ofs + 0] = _c.x;
            if (m_components > 1) data[ofs + 1] = _c.y;
            if (m_components > 2) data[ofs + 2] = _c.z;
            if (m_components > 3) data[ofs + 3] = _c.w;
        }

        glm::vec4 getPixelFloat(int _x, int _y) const
        {
            int ofs = m_components * (_y * m_width + _x);
            const float* data = (const float*)(m_data.data());
            return glm::vec4(
                m_components > 0 ? data[ofs + 0] : 0.0f,
                m_components > 1 ? data[ofs + 1] : 0.0f,
                m_components > 2 ? data[ofs + 2] : 0.0f,
                m_components > 3 ? data[ofs + 3] : 0.0f);
        }

        void setPixelUnsignedByte(int _x, int _y, glm::vec4& _c)
        {
            int ofs = m_components * (_y * m_width + _x);
            if (m_components > 0) m_data[ofs + 0] = uint8_t(_c.x * 255.0f);
            if (m_components > 1) m_data[ofs + 1] = uint8_t(_c.y * 255.0f);
            if (m_components > 2) m_data[ofs + 2] = uint8_t(_c.z * 255.0f);
            if (m_components > 3) m_data[ofs + 3] = uint8_t(_c.w * 255.0f);
        }

        glm::vec4 getPixelUnsignedByte(int _x, int _y) const
        {
            int ofs = m_components * (_y * m_width + _x);
            return glm::vec4(
                m_components > 0 ? float(m_data[ofs + 0]) / 255.0f : 0.0f,
                m_components > 1 ? float(m_data[ofs + 1]) / 255.0f : 0.0f,
                m_components > 2 ? float(m_data[ofs + 2]) / 255.0f : 0.0f,
                m_components > 3 ? float(m_data[ofs + 3]) / 255.0f : 0.0f);
        }
    };
}