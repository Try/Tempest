#include "font.h"

#include <Tempest/Except>
#include <Tempest/Painter>
#include <Tempest/Platform>
#include <Tempest/Log>
#include "../utility/utf8_helper.h"
#include "thirdparty/stb_truetype.h"

#include <unordered_map>
#include <bitset>
#include <algorithm>

#ifdef __WINDOWS__
#include <Shlobj.h>
#endif

using namespace Tempest;

namespace Tempest {
namespace Detail {
  template<size_t lvl>
  struct Bucket {
    std::unique_ptr<Bucket<lvl-1>> next[256];
    };

  template<>
  struct Bucket<1> {
    FontElement::Letter     letter[256];
    std::bitset<256>        ex;
    };

  static std::string getFontFolderPath() {
#ifdef __WINDOWS__
    char   path[MAX_PATH]={};
    SHGetSpecialFolderPathA(nullptr,path,CSIDL_FONTS,false);
    return path;
#else
    return "";
#endif
    }

  static std::string getFallbackFont() {
#ifdef __WINDOWS__
    return getFontFolderPath()+"\\georgia.ttf";
#else
    return "";
#endif
    }
  }

}

struct FontElement::LetterTable {
  struct Chunk {
    uint32_t          size=0;
    Detail::Bucket<3> next[256];
    };

  std::vector<Chunk> chunk;

  Letter& at(float sz,char32_t ch){
    return *implFind(sz,ch,true);
    }

  Letter* find(float sz,char32_t ch){
    return implFind(sz,ch,false);
    }

  Letter* implFind(float sz,char32_t ch,bool create){
    uint32_t size = uint32_t(sz*100);

    for(auto& i:chunk)
      if(i.size==size)
        return implFind(i,uint32_t(ch),create);

    chunk.emplace_back();
    chunk.back().size=size;
    return implFind(chunk.back(),uint32_t(ch),create);
    }

  Letter* implFind(Chunk& c,uint32_t ch,bool create){
    uint8_t* key = reinterpret_cast<uint8_t*>(&ch);
    auto&    b   = c.next[*key];
    return implFind(b,key+1,create);
    }

  template<size_t lvl>
  Letter* implFind(Detail::Bucket<lvl>& b,const uint8_t* k,bool create){
    auto& ptr = b.next[*k];
    if(ptr==nullptr && create)
      ptr.reset(new Detail::Bucket<lvl-1>());
    if(ptr==nullptr)
      return nullptr;
    return implFind(*b.next[*k],k+1,create);
    }

  Letter* implFind(Detail::Bucket<1>& b,const uint8_t* pk,bool create){
    auto k = *pk;
    if(!b.ex[k]){
      if(create){
        b.ex[k]=true;
        return &b.letter[k];
        }
      return nullptr;
      }
    return &b.letter[k];
    }
  };

struct FontElement::Impl {
  enum { MIN_BUF_SZ=512 };

  template<class CharT>
  Impl(const CharT *filename) {
    if(filename==nullptr)
      return;

    RFile file(filename);
    size = uint32_t(file.size());
    data = new uint8_t[size];

    if(file.read(data,size)!=size) {
      delete[] data;
      throw std::system_error(Tempest::SystemErrc::UnableToLoadAsset);
      }
    if(stbtt_InitFont(&info,data,0)==0) {
      delete[] data;
      throw std::system_error(Tempest::SystemErrc::UnableToLoadAsset);
      }
    stbtt_GetFontVMetrics(&info,&metrics0.ascent,&metrics0.descent,&lineGap);
    }

  ~Impl() {
    delete[] data;
    std::free(rasterBuf);
    }

  uint8_t* ttfMalloc(size_t sz){
    if(sz<rasterSz)
      return rasterBuf;
    sz = std::max<size_t>(sz,MIN_BUF_SZ);
    void* p=std::realloc(rasterBuf,sz);
    if(p==nullptr)
      return nullptr;
    rasterBuf=reinterpret_cast<uint8_t*>(p);
    rasterSz =sz;
    return rasterBuf;
    }

  uint8_t* getGlyphBitmapSubpixel(stbtt_fontinfo *info,
                                  float scale,int  glyph,
                                  int&  width,int& height,
                                  int&  xoff, int& yoff) {
    assert(scale>0.f);

    stbtt_vertex *vertices;
    int num_verts = stbtt_GetGlyphShape(info, glyph, &vertices);

    int ix0,iy0,ix1,iy1;
    stbtt_GetGlyphBitmapBoxSubpixel(info, glyph, scale, scale, 0.f/*shift_x*/, 0.f/*shift_y*/, &ix0,&iy0,&ix1,&iy1);

    stbtt__bitmap gbm={};
    // now we get the size
    gbm.w = (ix1 - ix0);
    gbm.h = (iy1 - iy0);

    width  = gbm.w;
    height = gbm.h;

    xoff   = ix0;
    yoff   = iy0;

    if(gbm.w>0 && gbm.h>0) {
      gbm.pixels = ttfMalloc(size_t(gbm.w*gbm.h));
      if(gbm.pixels!=nullptr) {
        gbm.stride = gbm.w;
        stbtt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale, scale, 0.f/*shift_x*/, 0.f/*shift_y*/, ix0, iy0, 1, info->userdata);
        }
      }

    STBTT_free(vertices,info->userdata);
    return gbm.pixels;
    }

  static const Letter& nullLater(){
    static const Letter l;
    return l;
    }

  const Letter& letter(char32_t ch,float size,TextureAtlas* tex) {
    {
    std::lock_guard<std::mutex> guard(syncMap);
    auto cc=map.find(size,ch);
    if(cc!=nullptr){
      if(cc->hasView || tex==nullptr)
        return *cc;
      }
    }

    if(this->size==0)
      return nullLater();
    return allocLetter(ch,size,tex,false);
    }

  const Letter& allocLetter(char32_t ch,float size,TextureAtlas* tex,bool fallback) {
    const float scale = stbtt_ScaleForPixelHeight(&info,size); //size/(ascent-descent);
    if(!(scale>0.f))
      return nullLater();

    int w=0,h=0,dx=0,dy=0;
    int ax=0;

    const int index = stbtt_FindGlyphIndex(&info,int(ch));
    stbtt_GetGlyphHMetrics(&info,index,&ax,nullptr);

    Sprite spr;
    if(tex!=nullptr){
      std::lock_guard<std::mutex> guard(syncMem);
      uint8_t* bitmap=getGlyphBitmapSubpixel(&info,scale,index,w,h,dx,dy);
      if(bitmap!=nullptr)
        spr = tex->load(bitmap,uint32_t(w),uint32_t(h),Pixmap::Format::R);
      } else {
      int ix0=0,ix1=0,iy0=0,iy1=0;
      stbtt_GetGlyphBitmapBoxSubpixel(&info,index,scale,scale,0.f,0.f,&ix0,&iy0,&ix1,&iy1);

      w  = (ix1 - ix0);
      h  = (iy1 - iy0);
      dx = ix0;
      dy = iy0;
      }

    if((w<=0 || h<=0) && ax==0) {
      if(!fallback)
        return allocFallbackLetter(ch,size,tex);
      return nullLater();
      }

    std::lock_guard<std::mutex> guard(syncMap);
    Letter& lt = map.at(size,ch);
    lt.view    = std::move(spr);
    lt.size    = Size(w,h);
    lt.dpos    = Point(dx,dy);
    lt.advance = Point(int(ax*scale),int(lineGap*scale));
    lt.hasView = (tex!=nullptr);
    return lt;
    }

  const Letter& allocFallbackLetter(char32_t ch,float size,TextureAtlas* tex) {
    try {
      std::lock_guard<std::mutex> guard(syncMap);
      if(fallback==nullptr){
        fallback.reset(new Impl(Detail::getFallbackFont().c_str()));
        if(stbtt_InitFont(&fallback->info,fallback->data,0)==0)
          throw std::system_error(Tempest::SystemErrc::UnableToLoadAsset);
        }

      Letter  lf = fallback->allocLetter(ch,size,tex,true);
      Letter& lt = map.at(size,ch);
      lt = lf;
      return lt;
      }
    catch (...) {
      return nullLater();
      }
    }

  Metrics       metrics(float size) const {
    if(this->size==0)
      return Metrics();
    const float scale = stbtt_ScaleForPixelHeight(&info,size);
    Metrics m = metrics0;
    m.ascent  = int(m.ascent*scale);
    m.descent = int(m.descent*scale);
    return m;
    }

  uint8_t*       data=nullptr;
  uint32_t       size=0;
  stbtt_fontinfo info={};

  std::mutex     syncMem;
  uint8_t*       rasterBuf=nullptr;
  size_t         rasterSz =0;
  Metrics        metrics0;
  int            lineGap=0;

  std::mutex                           syncMap;
  LetterTable                          map;
  std::unique_ptr<Impl>                fallback;
  };

FontElement::FontElement() {
  static std::shared_ptr<Impl> dummy = std::make_shared<Impl>(static_cast<const char*>(nullptr));
  ptr = dummy;
  }

FontElement::FontElement(std::nullptr_t)
  :FontElement(){
  }

template<class CharT>
FontElement::FontElement(const CharT *file,std::true_type)
  :ptr(std::make_shared<Impl>(file)) {
  }

FontElement::FontElement(const char *file)
  :FontElement(file,std::true_type()) {
  }

FontElement::FontElement(const char16_t *file)
  :FontElement(file,std::true_type()) {
  }

FontElement::FontElement(const std::string& file)
  :FontElement(file.c_str(),std::true_type()) {
  }

FontElement::FontElement(const std::u16string& file)
  :FontElement(file.c_str(),std::true_type()) {
  }

const FontElement::LetterGeometry& FontElement::letterGeometry(char32_t ch, float size) const { //FIXME: UB?
  return reinterpret_cast<const LetterGeometry&>(ptr->letter(ch,size,nullptr));
  }

const FontElement::Letter& FontElement::letter(char32_t ch, float size, TextureAtlas &tex) const {
  return ptr->letter(ch,size,&tex);
  }

Size FontElement::textSize(const char *text,float fontSize) const {
  Utf8Iterator i(text,std::strlen(text));

  Size ret;
  int  minY = 0;
  while(i.hasData()){
    char32_t c = i.next();
    if(c=='\0')
      return ret;
    auto& g = letterGeometry(c,fontSize);

    ret.h = std::max(ret.h,-g.dpos.y);
    minY  = std::min(minY, -g.dpos.y-g.size.h);
    ret.w += g.advance.x;
    }

  ret.h+=minY;
  return ret;
  }

bool FontElement::isEmpty() const {
  return ptr->size==0;
  }

FontElement::Metrics FontElement::metrics(float size) const {
  return ptr->metrics(size);
  }

template<class CharT>
Font::Font(const CharT *file,std::true_type)
  : fnt{{file,nullptr},{nullptr,nullptr}}{
  fnt[1][0]=fnt[0][0];
  fnt[1][1]=fnt[0][0];
  fnt[0][1]=fnt[0][0];
  }

Font::Font(const char *file)
  : Font(file,std::true_type()){
  }

Font::Font(const std::string &file)
  : Font(file.c_str(),std::true_type()){
  }

Font::Font(const char16_t *file)
  : Font(file,std::true_type()){
  }

Font::Font(const std::u16string &file)
  : Font(file.c_str(),std::true_type()){
  }

Font::Font(const FontElement& regular, const FontElement& bold,
           const FontElement& italic, const FontElement& boldItalic) {
  fnt[0][0] = regular;
  fnt[1][0] = bold;
  fnt[1][1] = boldItalic;
  fnt[0][1] = italic;
  }

void Font::setPixelSize(float sz) {
  size=sz;
  }

void Font::setBold(bool b) {
  bold = b ? 1 : 0;
  }

bool Font::isBold() const {
  return bold!=0;
  }

void Font::setItalic(bool i) {
  italic = i ? 1: 0;
  }

bool Font::isItalic() const {
  return italic;
  }

bool Font::isEmpty() const {
  return fnt[0][0].isEmpty() || fnt[0][1].isEmpty() ||
         fnt[1][0].isEmpty() || fnt[1][1].isEmpty();
  }

Font::Metrics Font::metrics() const {
  return fnt[bold][italic].metrics(size);
  }

const Font::LetterGeometry &Font::letterGeometry(char16_t ch) const {
  return fnt[bold][italic].letterGeometry(ch,size);
  }

const Font::LetterGeometry &Font::letterGeometry(char32_t ch) const {
  return fnt[bold][italic].letterGeometry(ch,size);
  }

const Font::Letter &Font::letter(char16_t ch, TextureAtlas &tex) const {
  return fnt[bold][italic].letter(ch,size,tex);
  }

const Font::Letter &Font::letter(char32_t ch, TextureAtlas &tex) const {
  return fnt[bold][italic].letter(ch,size,tex);
  }

const Font::Letter &Font::letter(char16_t ch, Painter &p) const {
  return letter(ch,p.ta);
  }

const Font::Letter &Font::letter(char32_t ch, Painter &p) const {
  return letter(ch,p.ta);
  }

Size Font::textSize(const char *text) const {
  return fnt[bold][italic].textSize(text,size);
  }

Size Font::textSize(const std::string &text) const {
  return textSize(text.c_str());
  }
