/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

#include <ImageIO/ImageIO.h>
#include <vector>
#include <string>
#include <set>
#include "OpenImageIO/imageio.h"
#include "OpenImageIO/color.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace  {

const static char kDepLibrary[] = "CoreGraphics.framework";

struct UtiExtension {
    const char* uti;
    const char* extensions;
};
const static UtiExtension sUtiExtensions[] = {
    { "com.adobe.pdf", "pdf\0" },
    { "com.adobe.photoshop-image", "psd\0" },
    { "com.adobe.raw-image", "dng\0" },
    { "com.apple.icns", "icns\0" },
    { "com.apple.macpaint-image", "mac\0ptng\0pnt" },
    { "com.apple.pict", "pict\0pct\0pic\0" },
    { "com.apple.quicktime-image", "qt\0mov\0qtif\0qti\0" },
    { "com.apple.rjpeg", "rjpeg\0" },
    { "com.canon.cr2-raw-image", "cr2\0" },
    { "com.canon.crw-raw-image", "crw\0" },
    { "com.canon.tif-raw-image", 0 },
    { "com.compuserve.gif", "gif\0" },
    { "com.dxo.raw-image", "dxo\0" },
    { "com.epson.raw-image", "erf\0" },
    { "com.fuji.raw-image", "raf\0" },
    { "com.hasselblad.3fr-raw-image", "3fr\0" },
    { "com.hasselblad.fff-raw-image", "fff\0" },
    { "com.ilm.openexr-image", "exr\0" },
    { "com.kodak.flashpix-image", "fpx\0fpix\0" },
    { "com.kodak.raw-image", "dcs\0dcr\0drf\0k25\0kdc\0" },
    { "com.konicaminolta.raw-image", "mrw\0" },
    { "com.leafamerica.raw-image", "mos\0" },
    { "com.leica.raw-image", "dng\0" },
    { "com.leica.rwl-raw-image", "rwl\0" },
    { "com.microsoft.bmp", "bmp\0BMPf\0" },
    { "com.microsoft.cur", "cur\0" },
    { "com.microsoft.ico", "ico\0" },
    { "com.nikon.nrw-raw-image", "nrw\0" },
    { "com.nikon.raw-image", "nef\0" },
    { "com.olympus.or-raw-image", "orf\0" },
    { "com.olympus.raw-image", 0 },
    { "com.olympus.sr-raw-image", "srw\0" },
    { "com.panasonic.raw-image", "raw\0" },
    { "com.panasonic.rw2-raw-image", "rw2\0" },
    { "com.pentax.raw-image", "pef\0ptx\0" },
    { "com.samsung.raw-image", "srw\0" },
    { "com.sgi.sgi-image", "sgi\0" },
    { "com.sony.arw-raw-image", "arw\0" },
    { "com.sony.raw-image", "srf\0" },
    { "com.sony.sr2-raw-image", "sr2\0" },
    { "com.truevision.tga-image", "tga\0targa\0" },
    { "public.jpeg", "jpg\0jpe\0jpeg\0" },
    { "public.jpeg-2000", "jp2\0j2k\0jpf\0jpx\0jpm\0mj2\0" },
    { "public.mpo-image", "mpo\0" },
    { "public.pbm", "pbm\0" },
    { "public.png", "png\0" },
    { "public.pvr", "pvr\0" },
    { "public.radiance", "hdr\0" },
    { "public.tiff", "tif\0tiff\0" }
};

class UtiExtensions {
    std::vector<const char*> mStorage;
    std::set<std::string> mStrings;

    struct FindUti {
        FindUti() {}
        bool operator () (const UtiExtension& a, const char* b) const {
            return ::strcmp(a.uti, b) < 0;
        }
        bool operator () (const char* a, const UtiExtension& b) const {
            return ::strcmp(a, b.uti) < 0;
        }
    };

    static bool findExtensions(const UtiExtension* begin, const UtiExtension* end,
                               const char** val) {
        const FindUti op;
        const UtiExtension* i = std::lower_bound(begin, end, *val, op);
        if (i != end && !op(*val,*i)) {
            *val = i->extensions;
            return true;
        }
        return false;
    }

    void appendExtensions(const char* extList) {
        while (extList[0])
            extList += mStrings.insert(extList).first->size() + 1;
    }

    void finish() {
        mStorage.reserve(mStrings.size()+1);
        for (std::set<std::string>::const_iterator it = mStrings.begin(), e = mStrings.end(); it != e; ++it) {
            mStorage.push_back(it->c_str());
        }
        // Terminate the list
        mStorage.push_back(NULL);
    }

public:
    UtiExtensions(CFArrayRef arrayRef) {
        std::vector<char> utiBuf;
        const UtiExtension *begin = &sUtiExtensions[0],
                           *end = begin + (sizeof(sUtiExtensions)/sizeof(sUtiExtensions[0]));
        
        for (size_t i = 0, n = CFArrayGetCount(arrayRef); i < n; ++i) {
            CFStringRef uti = (CFStringRef) CFArrayGetValueAtIndex(arrayRef, i);
            assert(CFGetTypeID(uti) == CFStringGetTypeID());

            const char* cStr = CFStringGetCStringPtr(uti, kCFStringEncodingASCII);
            if (!cStr) {
                utiBuf.resize(CFStringGetLength(uti) + 1);
                if (CFStringGetCString(uti, &utiBuf[0], utiBuf.size(),
                                       kCFStringEncodingASCII)) {
                    fprintf(stderr, "Ignoring UTI: ");
                    CFShow(uti);
                    continue;
                }
                cStr = &utiBuf[0];
            }
            if (findExtensions(begin, end, &cStr)) {
                if (cStr)
                    appendExtensions(cStr);
            } else
                fprintf(stderr, "Unkown UTI: '%s'\n", cStr);
        }
        CFRelease(arrayRef);

        // EPS has no UTI?
        appendExtensions("eps\0epi\0epsf\0epsi\0ps\0");
        appendExtensions("xbm\0cur\0");

        finish();
    }
    operator const char** () { return &mStorage[0]; }
};



template <class T>
class CFObject {
protected:
    T m_ref;
public:
    CFObject(CFTypeRef ref = NULL) : m_ref((T)ref) {}
    ~CFObject() { reset(); }

    T ref() const { return m_ref; }
    operator T () const { return ref(); }
    void operator = (T ref) { reset(ref); }

    void reset(T ref = NULL) {
        if (m_ref)
            CFRelease(m_ref);
        m_ref = ref;
    }
};

class CFString : public CFObject<CFStringRef> {
public:
    CFString(CFStringRef ref = NULL) : CFObject(ref) {}
    CFString(const char* bytes) {
        m_ref = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)bytes, strlen(bytes), kCFStringEncodingASCII, false);
    }
    CFString(const std::string &str) {
        m_ref = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)str.data(), str.size(), kCFStringEncodingASCII, false);
    }
};

class CFDictionary : public CFObject<CFDictionaryRef> {
public:
    CFDictionary(CFDictionaryRef ref = NULL) : CFObject(ref) {}
    bool has(const CFString& key) {
        return CFDictionaryContainsKey(m_ref, key);
    }
    template <class T> T
    get(const CFString& key, T dflt = 0) const;
};

template <>
int CFDictionary::get<int>(const CFString& key, int value) const {
    CFObject<CFNumberRef> number(CFDictionaryGetValue(m_ref, key));
    if (number)
        CFNumberGetValue(number, kCFNumberIntType, &value);
    return value;
}

class CoreGraphicsInput : public ImageInput {
    mutable CFObject<CGImageSourceRef> m_source;
    CFObject<CGImageRef> m_image;
    CFObject<CFDataRef> m_data;
    int m_subimage;

 public:
    CoreGraphicsInput () : m_subimage(0) { }
    virtual ~CoreGraphicsInput () { close(); }
    virtual const char * format_name (void) const {
        return "coregraphics";
    }

    virtual bool valid_file (const std::string &filename) const {
        if (CFURLRef urlRef = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8*)filename.c_str(), filename.size(), false)) {
            const_cast<CoreGraphicsInput*>(this)->close();
            m_source = CGImageSourceCreateWithURL(urlRef, NULL);
            CFRelease(urlRef);
            return m_source.ref() != NULL;
        }
        return false;
    }

    virtual bool open (const std::string &name, ImageSpec &spec) {
        if (!valid_file(name))
            return false;

        m_subimage = -1;
        return seek_subimage(0, 0, spec);
    }

    virtual bool read_native_scanlines (int ybegin, int yend, int z,
                                        void *data) {
        if (!m_data) {
            m_data = CGDataProviderCopyData(CGImageGetDataProvider(m_image));
            if (!m_data)
                return false;
        }
        const size_t rowbytes = CGImageGetBytesPerRow(m_image);
        assert(rowbytes == (m_spec.width * m_spec.nchannels * CGImageGetBitsPerComponent(m_image)/8));

        const size_t imgLen = CFDataGetLength(m_data);
        const size_t start  = ybegin * rowbytes;
        if (start > imgLen) {
            error("Requested data out of range: %lu : %lu\n", start, imgLen);
            return false;
        }
        const size_t end  = (yend-ybegin) * rowbytes;
        if (end > imgLen) {
            error("Requested more data than available: %lu : %lu\n", end, imgLen);
            return false;
        }
        
        CFDataGetBytes(m_data, CFRangeMake(start, end), (UInt8*)data);
        return true;
    }

    virtual bool read_native_scanline (int y, int z, void *data) {
        return read_native_scanlines(y, y+1, z, data);
    }
    
    virtual int current_subimage (void) const {
        return m_subimage;
    }

    virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec) {
        if (subimage < 0 || miplevel != 0)
            return false;
        
        if (m_subimage == subimage) {
            // We're already pointing to the right subimage
            newspec = m_spec;
            return true;
        }

        if (m_subimage > subimage) {
            // requested subimage is located before the current one
            m_data.reset();
            m_image.reset();
        }
        if (subimage >= CGImageSourceGetCount(m_source)) {
            return false;
        }

        m_image = CGImageSourceCreateImageAtIndex(m_source, subimage, NULL);
        newspec = ImageSpec(CGImageGetWidth(m_image), CGImageGetHeight(m_image), TypeDesc::UNKNOWN);
        const CGBitmapInfo info = CGImageGetBitmapInfo(m_image);
        switch (CGImageGetBitsPerComponent(m_image)) {
            case 8:
                newspec.set_format(TypeDesc::UINT8);
                break;
            case 16:
                newspec.set_format(info & kCGBitmapFloatComponents ? TypeDesc::HALF : TypeDesc::UINT16);
                break;
            case 32:
                newspec.set_format(info & kCGBitmapFloatComponents ? TypeDesc::FLOAT : TypeDesc::UINT32);
                break;
            default:
                return false;
        }

        newspec.nchannels = 4;
        switch (CGImageGetAlphaInfo(m_image)) {
            case kCGImageAlphaNone:
                newspec.nchannels = 3;

            case kCGImageAlphaNoneSkipLast:
            case kCGImageAlphaNoneSkipFirst:
                newspec.alpha_channel = -1;
                break;

            case kCGImageAlphaLast:
            case kCGImageAlphaPremultipliedLast:
                newspec.alpha_channel = 4;
                break;

            case kCGImageAlphaFirst:
            case kCGImageAlphaPremultipliedFirst:
                newspec.alpha_channel = 0;
                break;

            case kCGImageAlphaOnly:
                newspec.nchannels = 1;
                newspec.alpha_channel = 0;
                break;
        }
        newspec.default_channel_names();

/*
       CGColorSpaceRef colorSpace = CGImageGetColorSpace(m_image);
       switch (CGColorSpaceGetModel(colorSpace)) {
            case kCGColorSpaceModelRGB: {
                CFString colorSpaceName(CGColorSpaceCopyName(colorSpace));
                if (colorSpaceName.ref() == kCGColorSpaceSRGB)
                    newspec.attribute("oiio:ColorSpace", "sRGB");
                else if (colorSpaceName.ref() == kCGColorSpaceGenericRGBLinear)
                    newspec.attribute("oiio:ColorSpace", "Linear");
            }
                break;

            case kCGColorSpaceModelMonochrome:
            case kCGColorSpaceModelCMYK:
            case kCGColorSpaceModelLab:
            case kCGColorSpaceModelDeviceN:
            case kCGColorSpaceModelIndexed:
            case kCGColorSpaceModelPattern:
            case kCGColorSpaceModelUnknown: {
                CFObject<CGColorSpaceRef> linear(CGColorSpaceCreateWithName(kCGColorSpaceGenericRGBLinear));
                m_image = CGImageCreateCopyWithColorSpace(m_image, linear);
                newspec.attribute("oiio:ColorSpace", "Linear");
            }
                break;
        }
*/
        CFDictionary props = CGImageSourceCopyProperties(m_source, NULL);
        if (const int loop = props.get<int>("LoopCount"))
            newspec.attribute("gif:LoopCount", loop);

        if (CGImageSourceGetCount(m_source) > 1)
            newspec.attribute ("oiio:Movie", 1);

        m_spec = newspec;
        m_subimage = subimage;
        return true;
    }

    virtual bool close () {
        m_data.reset();
        m_image.reset();
        m_source.reset();
        return true;
    }
};

} // anonymous namespace

OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int coregraphics_imageio_version = OIIO_PLUGIN_VERSION;
OIIO_EXPORT const char *coregraphics_input_extensions[] = { 0 };
OIIO_EXPORT const char *coregraphics_output_extensions[] = { 0 };

OIIO_EXPORT ImageInput *coregraphics_input_imageio_create () {
    return new CoreGraphicsInput;
}
OIIO_EXPORT ImageOutput *coregraphics_output_imageio_create () {
    return NULL;
}

OIIO_EXPORT const char* coregraphics_imageio_library_version () {
    //
    // Register extensions explicity as there is a problem passing a dynmically
    // generated extension lists through a variable (coregraphics_input_extensions).
    //
    // This works because coregraphics_imageio_library_version will be called before
    // declare_imageio_format in imageioplugin.cpp

    // Generate all the extensions known to CoreGraphics from it's UTIs.
    UtiExtensions inputExts(CGImageSourceCopyTypeIdentifiers());
    //UtiExtensions outputExts(CGImageDestinationCopyTypeIdentifiers());

    declare_imageio_format("coregraphics", coregraphics_input_imageio_create,
                           inputExts, NULL, NULL, kDepLibrary, 65);
    return kDepLibrary;
}

OIIO_PLUGIN_EXPORTS_END


OIIO_PLUGIN_NAMESPACE_END
