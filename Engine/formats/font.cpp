#include "font.h"

#include <Tempest/Except>

#include "thirdparty/stb_truetype.h"

#include <unordered_map>

using namespace Tempest;

struct FontElement::Impl {
  struct Key {
    float    size;
    char16_t ch;

    bool operator==(const Key& k) const { return size==k.size && ch==k.ch; }
    };

  struct KHash {
    size_t operator()(const Key& k) const {
      return size_t(k.size)^size_t(k.ch);
      }
    };

  template<class CharT>
  Impl(const CharT *filename) {
    if(filename==nullptr)
      return;

    RFile file(filename);
    size = file.size();
    data = new uint8_t[size];

    if(file.read(data,size)!=size) {
      delete data;
      throw std::bad_alloc(); //TODO
      }
    }

  ~Impl() {
    delete data;
    }

  const Letter& letter(char16_t ch,float size,TextureAtlas* tex) {
    auto cc=map.find(Key{size,ch});
    if(cc!=map.end()){
      if(cc->second.hasView || tex==nullptr)
        return cc->second;
      }

    if(this->size==0){
      static const Letter l;
      return l;
      }

    int w=0,h=0, dx=0,dy=0;
    int ax=0;
    float    scale  = stbtt_ScaleForPixelHeight(&info,size);
    stbtt_GetCodepointHMetrics(&info,ch,&ax,nullptr);
    uint8_t* bitmap = tex==nullptr ? nullptr : stbtt_GetCodepointBitmap (&info, 0,scale, ch, &w, &h, &dx, &dy);

    Letter src;
    try {
      if(bitmap!=nullptr) {
        src.view = tex->load(bitmap,uint32_t(w),uint32_t(h),Pixmap::Format::A);
        stbtt_FreeBitmap(bitmap,nullptr);
        }

      auto ins = map.insert(std::make_pair(Key{size,ch},std::move(src)));
      Letter& lt = ins.first->second;
      lt.size    = Size(w,h);
      lt.dpos    = Point(dx,dy);
      lt.advance = Point(int(ax*scale),int(lineGap*scale));
      lt.hasView = (tex!=nullptr);
      return lt;
      }
    catch(...) {
      if(bitmap!=nullptr)
        stbtt_FreeBitmap(bitmap,nullptr);
      throw;
      }
    }

  uint8_t*       data=nullptr;
  uint32_t       size=0;
  stbtt_fontinfo info={};

  int ascent =0;
  int descent=0;
  int lineGap=0;

  std::unordered_map<Key,Letter,KHash> map;
  };

FontElement::FontElement()
  :FontElement(static_cast<const char*>(nullptr)){
  }

FontElement::FontElement(std::nullptr_t)
  :FontElement(){
  }

template<class CharT>
FontElement::FontElement(const CharT *file,std::true_type)
  :ptr(std::make_shared<Impl>(file)) {
  if(file) {
    if(stbtt_InitFont(&ptr->info,ptr->data,0)==0)
      throw std::system_error(Tempest::SystemErrc::UnableToLoadAsset);
    stbtt_GetFontVMetrics(&ptr->info,&ptr->ascent,&ptr->descent,&ptr->lineGap);
    }
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

const FontElement::LetterGeometry& FontElement::letterGeometry(char16_t ch, float size) const { //FIXME: UB?
  return reinterpret_cast<const LetterGeometry&>(ptr->letter(ch,size,nullptr));
  }

const FontElement::Letter& FontElement::letter(char16_t ch, float size, TextureAtlas &tex) const {
  return ptr->letter(ch,size,&tex);
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

void Font::setPixelSize(float sz) {
  size=sz;
  }

const Font::LetterGeometry &Font::letterGeometry(char16_t ch) const {
  return fnt[0][0].letterGeometry(ch,size);
  }

const Font::Letter &Font::letter(char16_t ch, TextureAtlas &tex) const {
  return fnt[0][0].letter(ch,size,tex);
  }
