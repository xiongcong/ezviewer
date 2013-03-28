#include "imagefactory.h"

#include "config.h"
#include "imagewrapper.h"
#include "toolkit.h"

#include <QRunnable>
#include <QThreadPool>


/* list:
 * first one hold curCache for PicManager,
 * second one is for pre-reading (if enabled),
 * others are caches for file that had been viewed.
 */
QList<ImageWrapper *> ImageFactory::list;
int ImageFactory::CacheNumber = 0;


class Runnable : public QRunnable
{
public:
    Runnable(ImageWrapper * iw, const QString &filePath)
        : image(iw), path(filePath) {}

    void run() {
        if (image)
            image->load(path, true);
    }

private:
    ImageWrapper *image;
    QString path;
};


ImageWrapper * ImageFactory::newOrReuseImage()
{
    int total = 1 + (Config::enablePreReading() ? 1 : 0) + CacheNumber;
    qDebug("cache num is %d, total is %d, now has %d",
           CacheNumber, total, list.size());

    ImageWrapper *image;
    if(list.size() < total){
        image = new ImageWrapper();
    }else{
        image = list.at(total - 1);
        list.removeAt(total - 1);
        image->recycle();
        image->setReady(false);
        image->setHashCode(ImageWrapper::HASH_INVALID);
    }

    return image;
}

ImageWrapper * ImageFactory::findImageByHash(uint hash)
{
    foreach(ImageWrapper *image, list){
        if(image->getHashCode() == hash){
            list.removeOne(image);
            waitForImageReady(image);
            return image;
        }
    }
    return NULL;
}

void ImageFactory::waitForImageReady(ImageWrapper *image)
{
    while(!image->getReady()){
        // wait for pre-reading in another thread
    }
}

ImageWrapper * ImageFactory::getImageWrapper(const QString &filePath)
{
    uint hash = filePath.isEmpty() ? ImageWrapper::HASH_INVALID :
                                     ToolKit::getFileHash(filePath);
    ImageWrapper *image;
    if((image = findImageByHash(hash))){
        list.prepend(image);
        // TODO: update first frame of svg animation format here (if it has been shown before).
        return image;
    }

    image = newOrReuseImage();
    list.prepend(image);
    image->setHashCode(hash);
    if (hash == ImageWrapper::HASH_INVALID) {
        image->setReady(true);
    } else {
        image->load(filePath);
    }

    return image;
}


void ImageFactory::preReading(const QString &filePath)
{
    if(!Config::enablePreReading())
        return;

    uint hash = ToolKit::getFileHash(filePath);
    ImageWrapper *image;
    if((image = findImageByHash(hash))){
        list.insert(1, image);  /// assert(list.size());
        return;
    }

    image = newOrReuseImage();
    list.insert(1, image);  /// assert(list.size());
    image->setHashCode(hash);
    if (hash == ImageWrapper::HASH_INVALID) {
        image->setReady(true);
    } else {
        QThreadPool::globalInstance()->start(new Runnable(image, filePath));
    }
}

void ImageFactory::freeAllCache()
{
    QThreadPool::globalInstance()->waitForDone();

    foreach(ImageWrapper *image, list)
        freeImage(image);
}

void ImageFactory::freeImage(ImageWrapper *image)
{
    waitForImageReady(image);
    delete image;
}

void ImageFactory::cacheSizeAdjusted()
{
    int total = 1 + (Config::enablePreReading() ? 1 : 0) + CacheNumber;
    while(list.size() > total){
        freeImage(list.last());
        list.removeLast();
    }
}

void ImageFactory::setCacheNumber(int val)
{
    if(val < 0 || CacheNumber == val) return;

    CacheNumber = val;
    cacheSizeAdjusted();
}

void ImageFactory::setPreReadingEnabled(bool /*enabled*/)
{
    cacheSizeAdjusted();
}
