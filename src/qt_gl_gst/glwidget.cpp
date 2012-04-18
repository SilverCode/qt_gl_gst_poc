#include "glwidget.h"

GLWidget::GLWidget(int argc, char *argv[], QWidget *parent) :
    QGLWidget(QGLFormat(QGL::DoubleBuffer | QGL::DepthBuffer | QGL::Rgba), parent),
    closing(false),
    brickProg(this)
{
    xRot = 0;
    yRot = 0;
    zRot = 0;
    fScale = 1.0;
    lastPos = QPoint(0, 0);

    Rotate = 1;
    xLastIncr = 0;
    yLastIncr = 0;
    fXInertia = -0.5;
    fYInertia = 0;

    clearColor = 0;
    stackVidQuads = false;
//    vidOnObject = false;
    currentModelEffect = ModelEffectFirst;

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(animate()));
    timer->start(20);

    grabKeyboard();
    // Need this for glut models:
    int zero = 0;
    glutInit(&zero, NULL);

    // Video shader effects vars
    ColourHilightRangeMin = QVector4D(0.0, 0.0, 0.0, 0.0);
    ColourHilightRangeMax = QVector4D(0.2, 0.2, 1.0, 1.0); // show shades of blue as they are
    alphaTextureLoaded = false;

    // Video pipeline
    for(int vidIx = 1; vidIx < argc; vidIx++)
    {
        videoLoc.push_back(QString(argv[vidIx]));
    }

    // Instantiate video pipeline for each filename specified
    for(int vidIx = 0; vidIx < this->videoLoc.size(); vidIx++)
    {
        this->gstThreads.push_back(new GstThread(vidIx, this->videoLoc[vidIx], SLOT(newFrame(int)), this));
        QObject::connect(this->gstThreads[vidIx], SIGNAL(finished(int)),
                         this, SLOT(gstThreadFinished(int)));
        QObject::connect(this, SIGNAL(closeRequested()),
                         this->gstThreads[vidIx], SLOT(stop()), Qt::QueuedConnection);
    }

    currentModel = ModelFirst;
    objModel = NULL;
}

GLWidget::~GLWidget()
{
}

// Arrays containing lists of shaders which can be linked and used together:
#define NUM_SHADERS_VIDI420NOEFFECT       2
const char *VidI420NoEffectShaderList[NUM_SHADERS_VIDI420NOEFFECT] =
{
    "shaders/noeffect.frag",
    "shaders/yuv2rgb.frag"
};

#define NUM_SHADERS_VIDI420COLOURHILIGHT       2
const char *VidI420ColourHilightShaderList[NUM_SHADERS_VIDI420COLOURHILIGHT] =
{
    "shaders/colourhilight.frag",
    "shaders/yuv2rgb.frag"
};

#define NUM_SHADERS_VIDI420ALPHAMASK       2
const char *VidI420AlphaMaskShaderList[NUM_SHADERS_VIDI420ALPHAMASK] =
{
    "shaders/alphamask.frag",
    "shaders/yuv2rgb.frag"
};

void GLWidget::initializeGL()
{
    QString verStr((const char*)glGetString(GL_VERSION));
    QStringList verNums = verStr.split(".");
    std::cout << "GL_VERSION major=" << verNums[0].toStdString() << " minor=" << verNums[1].toStdString() << "\n";

    if(verNums[0].toInt() < 2)
    {
        qCritical("Support for OpenGL 2.0 is required for this demo...exiting\n");
        close();
    }

    std::cout << "Window is" << ((this->format().doubleBuffer()) ? "": " not") << " double buffered\n";

    qglClearColor(QColor(Qt::black));

    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#if 1//0
// no shader:

    /* Lighting Variables */
    GLfloat light_ambient[] = { 0.0, 0.0, 0.0, 1.0 };
    GLfloat light_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat light_specular[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat light_position[] = { 1.0, 1.0, 1.0, 0.0 };

    GLfloat mat_ambient[] = { 0.7, 0.7, 0.7, 1.0 };
    GLfloat mat_diffuse[] = { 0.8, 0.8, 0.8, 1.0 };
    GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat high_shininess[] = { 100.0 };

    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess);

    //glEnable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_MULTISAMPLE);
    static GLfloat lightPosition[4] = { 0.5, 5.0, 7.0, 1.0 };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glEnable(GL_NORMALIZE);
//#else
// shader:
    setupShader(&brickProg, "shaders/brick", true, true);
    // Set up initial uniform values
    brickProg.setUniformValue("BrickColor", QVector3D(1.0, 0.3, 0.2));
    brickProg.setUniformValue("MortarColor", QVector3D(0.85, 0.86, 0.84));
    brickProg.setUniformValue("BrickSize", QVector3D(0.30, 0.15, 0.30));
    brickProg.setUniformValue("BrickPct", QVector3D(0.90, 0.85, 0.90));
    brickProg.setUniformValue("LightPosition", QVector3D(0.0, 0.0, 4.0));
    brickProg.release();

    //setupShader(&I420NoEffect, "yuv2rgb-standalone", false, true);
    setupShader(&I420NoEffect, VidI420NoEffectShaderList, NUM_SHADERS_VIDI420NOEFFECT);
    setupShader(&I420ColourHilight, VidI420ColourHilightShaderList, NUM_SHADERS_VIDI420COLOURHILIGHT);
    setupShader(&I420AlphaMask, VidI420AlphaMaskShaderList, NUM_SHADERS_VIDI420ALPHAMASK);

    // set uniforms for vid shaders along with other stream details when first
    // frame comes through
#endif

    // Create entry in tex info vector for all pipelines
    for(int vidIx = 0; vidIx < this->gstThreads.size(); vidIx++)
    {
        VidTextureInfo newInfo;
        glGenTextures(1, &newInfo.texId);
        newInfo.texInfoValid = false;
        newInfo.buffer = NULL;
        newInfo.effect = VidShaderNormal;

        this->vidTextures.push_back(newInfo);
    }

    for(int vidIx = 0; vidIx < this->gstThreads.size(); vidIx++)
    {
        this->gstThreads[vidIx]->start();
    }



    objModel = glmReadOBJ(DFLT_OBJ_MODEL_FILE_NAME);
    printOpenGLError(__FILE__, __LINE__);
    if (!objModel)
    {
        qCritical() << "Couldn't load obj model file " << DFLT_OBJ_MODEL_FILE_NAME;
        currentModel = (ModelType) ((int) currentModel + 1);
    }
    else
    {
        glmUnitize(objModel);
        printOpenGLError(__FILE__, __LINE__);
        glmVertexNormals(objModel, 90.0, GL_TRUE);
        printOpenGLError(__FILE__, __LINE__);
    }
}

void GLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw a sky box - nah
    //drawSky();

    glLoadIdentity();
    glTranslatef(0.0, 0.0, -5.0);

    glRotatef(zRot / 16.0, 0.0, 0.0, 1.0);
    glRotatef(xRot / 16.0, 1.0, 0.0, 0.0);
    glRotatef(yRot / 16.0, 0.0, 1.0, 0.0);

    glScalef(fScale, fScale, fScale);


    // Draw an object in the middle
    ModelEffectType enabledModelEffect = currentModelEffect;
    switch(enabledModelEffect)
    {
    case ModelEffectNone:
        glmUseOtherTexId(objModel, 0, 0);
        //glUseProgram(0);
        brickProg.release();
        break;
    case ModelEffectBrick:
        brickProg.bind();
        break;
    case ModelEffectVideo:
        glActiveTexture(GL_TEXTURE0_ARB);
        //glEnable(GL_TEXTURE_RECTANGLE_ARB);
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->vidTextures[0].texId);

        this->vidTextures[0].shader->bind();
        setVidShaderVars(0, false);

        glmUseOtherTexId(objModel, 1, this->vidTextures[0].texId);
        break;
    }
 #if 0
    if(vidOnObject)
    {
        glActiveTexture(GL_TEXTURE0_ARB);
        glEnable(GL_TEXTURE_RECTANGLE_ARB);
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->vidTextures[0].texId);

        this->vidTextures[0].shader->bind();
        setVidShaderVars(0, false);
    }
    else
        brickProg.bind();
#endif
    switch( currentModel )
    {
        case ModelLoadedObj:
            glmDraw(objModel, GLM_SMOOTH | GLM_MATERIAL | GLM_TEXTURE);
            printOpenGLError(__FILE__, __LINE__);
            break;
        case ModelTeapot:
            glutSolidTeapot(0.6f);
            break;
        case ModelTorus:
            glutSolidTorus(0.2f, 0.6f, 64, 64);
            break;
        case ModelSphere:
            glutSolidSphere(0.6f, 64, 64);
            break;
        default:
            glutSolidTeapot(0.6f);
            break;
    }

    switch(enabledModelEffect)
    {
    case ModelEffectNone:
        break;
    case ModelEffectBrick:
        brickProg.release();
        break;
    case ModelEffectVideo:
        printOpenGLError(__FILE__, __LINE__);
        break;
    }
#if 0
    if(vidOnObject)
        printOpenGLError(__FILE__, __LINE__);
    else
        brickProg.release();
#endif

    // Draw videos around the object
    for(int vidIx = 0; vidIx < this->vidTextures.size(); vidIx++)
    {
        if(this->vidTextures[vidIx].texInfoValid)
        {
            // render a quad with the video on it:

            glActiveTexture(GL_TEXTURE0_ARB);
            //glEnable(GL_TEXTURE_RECTANGLE_ARB);
            glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->vidTextures[vidIx].texId);
            printOpenGLError(__FILE__, __LINE__);

            if((this->vidTextures[vidIx].effect == VidShaderAlphaMask) && this->alphaTextureLoaded)
            {
                glEnable (GL_BLEND);
                glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glActiveTexture(GL_TEXTURE1_ARB);
                //glEnable(GL_TEXTURE_RECTANGLE_ARB);
                glBindTexture(GL_TEXTURE_RECTANGLE_ARB, this->alphaTextureId);
            }

            this->vidTextures[vidIx].shader->bind();
            setVidShaderVars(vidIx, false);
            printOpenGLError(__FILE__, __LINE__);

            GLfloat width = this->vidTextures[vidIx].width;
            GLfloat height = this->vidTextures[vidIx].height;

            glPushMatrix();
            if(stackVidQuads)
            {
                glTranslatef(0.0, 0.0, 2.0);
                glTranslatef(0.0, 0.0, 0.2*vidIx);
            }
            else
            {
                glRotatef((360/this->vidTextures.size())*vidIx, 0.0, 1.0, 0.0);
                glTranslatef(0.0, 0.0, 2.0);
            }

            glBegin(GL_QUADS);
                glMultiTexCoord2fARB(GL_TEXTURE0_ARB, width, 0.0f);
                glMultiTexCoord2fARB(GL_TEXTURE1_ARB, alphaTexWidth, 0.0f);
                glVertex2f(-1.3f, 1.0f);
                glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0.0f, 0.0f);
                glMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0.0f, 0.0f);
                glVertex2f( 1.3f, 1.0f);
                glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0.0f, height);
                glMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0.0f, alphaTexHeight);
                glVertex2f( 1.3f, -1.0f);
                glMultiTexCoord2fARB(GL_TEXTURE0_ARB, width, height);
                glMultiTexCoord2fARB(GL_TEXTURE1_ARB, alphaTexWidth, alphaTexHeight);
                glVertex2f(-1.3f, -1.0f);
            glEnd();
            glPopMatrix();
        }

    }
}

void GLWidget::resizeGL(int wid, int ht)
{
    float vp = 0.8f;
    float aspect = (float) wid / (float) ht;

    glViewport(0, 0, wid, ht);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    //glOrtho(-1.0, 1.0, -1.0, 1.0, -10.0, 10.0);
    glFrustum(-vp, vp, -vp / aspect, vp / aspect, 1.0, 50.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -5.0);
}

void GLWidget::newFrame(int vidIx)
{
    if(this->gstThreads[vidIx])
    {

        Pipeline *pipeline = this->gstThreads[vidIx]->getPipeline();
        if(!pipeline)
          return;

        /* vid frame pointer is initialized as null */
        if (this->vidTextures[vidIx].buffer)
            pipeline->queue_output_buf.put(this->vidTextures[vidIx].buffer);

        this->vidTextures[vidIx].buffer = pipeline->queue_input_buf.get();

        this->makeCurrent();

        // load the gst buf into a texture
        if(this->vidTextures[vidIx].texInfoValid == false)
        {
            // try and keep this fairly portable to other media frameworks by
            // leaving info extraction within pipeline class
            this->vidTextures[vidIx].width = pipeline->getWidth();
            this->vidTextures[vidIx].height = pipeline->getHeight();
            this->vidTextures[vidIx].colourFormat = pipeline->getColourFormat();
            this->vidTextures[vidIx].texInfoValid = true;

            setAppropriateVidShader(vidIx);

            this->vidTextures[vidIx].shader->bind();
            printOpenGLError(__FILE__, __LINE__);
            // Setting shader variables here will have no effect as they are set on every render,
            // but do it to check for errors, so we don't need to check on every render
            // and program output doesn't go mad
            setVidShaderVars(vidIx, true);
        }


        glBindTexture (GL_TEXTURE_RECTANGLE_ARB, this->vidTextures[vidIx].texId);

        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

        // TODO: move gst macro into pipeline class, have queue contain just pointer
        // to actual frame data
        glTexImage2D  (GL_TEXTURE_RECTANGLE_ARB, 0, GL_LUMINANCE,
                       this->vidTextures[vidIx].width,
                       1.5f*this->vidTextures[vidIx].height,
                       0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                       GST_BUFFER_DATA(this->vidTextures[vidIx].buffer));

        printOpenGLError(__FILE__, __LINE__);

        /* direct call to paintGL (no queued) */
        this->updateGL();
    }
}

void GLWidget::gstThreadFinished(int vidIx)
{
    if(this->closing)
    {
        delete(this->gstThreads[vidIx]);
        this->gstThreads.replace(vidIx, NULL);
        this->vidTextures[vidIx].texInfoValid = false;

        // check if any gst threads left, if not close
        bool allFinished = true;
        for(int i = 0; i < this->gstThreads.size(); i++)
        {
            if(this->gstThreads[i] != NULL)
            {
                // catch any threads which were already finished at quitting time
                if(this->gstThreads[i]->isFinished())
                {
                    delete(this->gstThreads[vidIx]);
                    this->gstThreads.replace(vidIx, NULL);
                    this->vidTextures[vidIx].texInfoValid = false;
                }
                else
                {
                    allFinished = false;
                    break;
                }
            }
        }
        if(allFinished)
        {
            close();
        }
    }
    else if(this->gstThreads[vidIx]->chooseNew())
    {
        // TODO: call a choosenewvid function here and do that from keyboard event handler if pipeline already stopped

        // Confirm that we have the new filename before we do anything else
        QString newFileName = QFileDialog::getOpenFileName(0, "Select a video file",
                                                           ".", "Videos (*.avi *.mkv *.ogg *.asf *.mov);;All (*.*)");

        if(newFileName.isNull() == false)
        {
            delete(this->gstThreads[vidIx]);
            this->vidTextures[vidIx].texInfoValid = false;

            this->videoLoc[vidIx] = newFileName;
            //this->makeCurrent();
            this->gstThreads[vidIx] =
              new GstThread(vidIx, this->videoLoc[vidIx], SLOT(newFrame(int)), this);
            //this->makeCurrent();

            QObject::connect(this->gstThreads[vidIx], SIGNAL(finished(int)),
                             this, SLOT(gstThreadFinished(int)));
            QObject::connect(this, SIGNAL(closeRequested()),
                             this->gstThreads[vidIx], SLOT(stop()), Qt::QueuedConnection);

            this->gstThreads[vidIx]->start();
        }
    }
    else
    {
        // TODO: restart video
    }
}

// Basic bits, input events, animation
QSize GLWidget::minimumSizeHint() const
{
    return QSize(50, 50);
}

QSize GLWidget::sizeHint() const
{
    return QSize(400, 400);
}

static int qNormalizeAngle(int angle)
{
    while (angle < 0)
        angle += 360 * 16;
    while (angle > 360 * 16)
        angle -= 360 * 16;

    return angle;
}

void GLWidget::nextClearColor(void)
{
    switch( clearColor++ )
    {
        case 0:  qglClearColor(QColor(Qt::black));
             break;
        case 1:  qglClearColor(QColor::fromRgbF(0.2f, 0.2f, 0.3f, 1.0f));
             break;
        default: qglClearColor(QColor::fromRgbF(0.7f, 0.7f, 0.7f, 1.0f));
             clearColor = 0;
             break;
    }
}

void GLWidget::animate()
{
    /* Increment wrt inertia */
    if (Rotate)
    {
        xRot = qNormalizeAngle(xRot + (8 * fYInertia));
        yRot = qNormalizeAngle(yRot + (8 * fXInertia));
    }

    updateGL();
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
    lastPos = event->pos();

    if (event->button() == Qt::LeftButton)
    {
        fXInertia = 0;
        fYInertia = 0;

        xLastIncr = 0;
        yLastIncr = 0;
    }
}

void GLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        // Left button released
        lastPos.setX(-1);
        lastPos.setY(-1);

        if (xLastIncr > INERTIA_THRESHOLD)
          fXInertia = (xLastIncr - INERTIA_THRESHOLD)*INERTIA_FACTOR;

        if (-xLastIncr > INERTIA_THRESHOLD)
          fXInertia = (xLastIncr + INERTIA_THRESHOLD)*INERTIA_FACTOR;

        if (yLastIncr > INERTIA_THRESHOLD)
          fYInertia = (yLastIncr - INERTIA_THRESHOLD)*INERTIA_FACTOR;

        if (-yLastIncr > INERTIA_THRESHOLD)
          fYInertia = (yLastIncr + INERTIA_THRESHOLD)*INERTIA_FACTOR;

    }
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    if((lastPos.x() != -1) && (lastPos.y() != -1))
    {
        xLastIncr = event->x() - lastPos.x();
        yLastIncr = event->y() - lastPos.y();

        if ((event->modifiers() & Qt::ControlModifier)
            || (event->buttons() & Qt::RightButton))
        {
           if (lastPos.x() != -1)
           {
               zRot = qNormalizeAngle(zRot + (8 * xLastIncr));
               fScale += (yLastIncr)*SCALE_FACTOR;
               updateGL();
           }
        }
        else
        {
           if (lastPos.x() != -1)
           {
               xRot = qNormalizeAngle(xRot + (8 * yLastIncr));
               yRot = qNormalizeAngle(yRot + (8 * xLastIncr));
               updateGL();
           }
        }
    }

    lastPos = event->pos();
}

void GLWidget::keyPressEvent(QKeyEvent *e)
{
    switch(e->key())
    {
        case Qt::Key_Question:
        case Qt::Key_H:
            std::cout <<  "\nKeyboard commands:\n\n"
                          "? - Help\n"
                          "q, <esc> - Quit\n"
                          "b - Toggle among background clear colors\n"
                          "t - Toggle among models to render\n"
                          "s - "
                          "a - "
                          "v - "
                          "o - "
                          "p - "
                          "<home>     - reset zoom and rotation\n"
                          "<space> or <click>        - stop rotation\n"
                          "<+>, <-> or <ctrl + drag> - zoom model\n"
                          "<arrow keys> or <drag>    - rotate model\n"
                          "\n";
            break;
        case Qt::Key_Escape:
        case Qt::Key_Q:
            close();
            break;

        case Qt::Key_B:
            nextClearColor();
            break;
        case Qt::Key_T:
            if (currentModel >= ModelLast)
                currentModel = ModelFirst;
            else
                currentModel = (ModelType) ((int) currentModel + 1);
            break;
        case Qt::Key_S:
            {
                int lastVidDrawn = this->vidTextures.size() - 1;
                if (this->vidTextures[lastVidDrawn].effect >= VidShaderLast)
                    this->vidTextures[lastVidDrawn].effect = VidShaderFirst;
                else
                    this->vidTextures[lastVidDrawn].effect = (VidShaderEffectType) ((int) this->vidTextures[lastVidDrawn].effect + 1);

                setAppropriateVidShader(lastVidDrawn);
                this->vidTextures[lastVidDrawn].shader->bind();
                printOpenGLError(__FILE__, __LINE__);
                // Setting shader variables here will have no effect as they are set on every render,
                // but do it to check for errors, so we don't need to check on every render
                // and program output doesn't go mad
                setVidShaderVars(lastVidDrawn, true);
            }
            break;
        case Qt::Key_A:
            {
                // Load an alpha mask texture. Get the filename before doing anything else
                QString alphaTexFileName = QFileDialog::getOpenFileName(0, "Select an image file",
                                                                        "./alphamasks/", "Pictures (*.bmp *.jpg *.jpeg *.gif);;All (*.*)");
                if(alphaTexFileName.isNull() == false)
                {
                    QImage alphaTexImage(alphaTexFileName);
                    if(alphaTexImage.isNull() == false)
                    {
                        // Ok, a new image is loaded
                        if(alphaTextureLoaded)
                        {
                            // delete the old texture
                            alphaTextureLoaded = false;
                            deleteTexture(alphaTextureId);
                        }

                        // bind new image to texture
                        alphaTextureId = bindTexture(alphaTexImage.mirrored(true, true), GL_TEXTURE_RECTANGLE_ARB);
                        alphaTexWidth = alphaTexImage.width();
                        alphaTexHeight = alphaTexImage.height();
                        alphaTextureLoaded = true;
                    }
                }
            }
            break;
        case Qt::Key_M:
            {
                // Load a Wavefront OBJ model file. Get the filename before doing anything else
                QString objFileName = QFileDialog::getOpenFileName(0, "Select a Wavefront OBJ file",
                                                                      "./models/", "Wavefront OBJ (*.obj)");
                if(objFileName.isNull() == false)
                {
                    this->makeCurrent();

                    glmDelete(objModel);
                    objModel = glmReadOBJ(objFileName.toAscii().constData());
                    if (!objModel)
                    {
                        qCritical() << "Couldn't load obj model file " << DFLT_OBJ_MODEL_FILE_NAME;
                        currentModel = (ModelType) ((int) currentModel + 1);
                    }
                    else
                    {
                        glmUnitize(objModel);
                        glmVertexNormals(objModel, 90.0, GL_TRUE);
                    }
                }
            }
            break;
        case Qt::Key_V:
            {
                int lastVidDrawn = this->vidTextures.size() - 1;
                this->gstThreads[lastVidDrawn]->setChooseNewOnFinished();
                this->gstThreads[lastVidDrawn]->stop();
            }
            break;
        case Qt::Key_O:
            //vidOnObject = !vidOnObject;
            if (currentModelEffect >= ModelEffectLast)
                currentModelEffect = ModelEffectFirst;
            else
                currentModelEffect = (ModelEffectType) ((int) currentModelEffect + 1);
            break;
        case Qt::Key_P:
            stackVidQuads = !stackVidQuads;
            break;

        case Qt::Key_Space:
            Rotate = !Rotate;

            if (!Rotate)
            {
                fXInertiaOld = fXInertia;
                fYInertiaOld = fYInertia;
            }
            else
            {
                fXInertia = fXInertiaOld;
                fYInertia = fYInertiaOld;

                // To prevent confusion, force some rotation
                if ((fXInertia == 0.0) && (fYInertia == 0.0))
                    fXInertia = -0.5;
            }
            break;
        case Qt::Key_Plus:
            fScale += SCALE_INCREMENT;
            break;
        case Qt::Key_Minus:
            fScale -= SCALE_INCREMENT;
            break;
        case Qt::Key_Home:
            xRot = 0;
            yRot = 35;
            zRot = 0;
            xLastIncr = 0;
            yLastIncr = 0;
            fXInertia = -0.5;
            fYInertia = 0;
            fScale    = 1.0;
        break;
        case Qt::Key_Left:
           yRot -= 8;
        break;
        case Qt::Key_Right:
           yRot += 8;
        break;
        case Qt::Key_Up:
           xRot -= 8;
        break;
        case Qt::Key_Down:
           xRot += 8;
        break;

        default:
            QGLWidget::keyPressEvent(e);
            break;
    }
}

void GLWidget::closeEvent(QCloseEvent* event)
{
    if(this->closing == false)
    {
        this->closing = true;
        emit closeRequested();

        // just in case, check now if any gst threads still exist, if not, close application now
        bool allFinished = true;
        for(int i = 0; i < this->gstThreads.size(); i++)
        {
            if(this->gstThreads[i] != NULL)
            {
                allFinished = false;
                break;
            }
        }
        if(allFinished)
        {
            close();
        }
        event->ignore();
    }
}

// Shader management
void GLWidget::setAppropriateVidShader(int vidIx)
{
    switch(this->vidTextures[vidIx].colourFormat)
    {
    case ColFmt_I420:
        switch(this->vidTextures[vidIx].effect)
        {
        case VidShaderNormal:
            this->vidTextures[vidIx].shader = &I420NoEffect;
            break;

        case VidShaderColourHilight:
            this->vidTextures[vidIx].shader = &I420ColourHilight;
            break;
        case VidShaderAlphaMask:
            this->vidTextures[vidIx].shader = &I420AlphaMask;
            break;
        }
        break;

    default:
        qDebug ("Haven't implemented a shader for colour format %d yet", this->vidTextures[vidIx].colourFormat);
        break;
    }
}

// Shader WILL be all set up for the specified video texture when this is called,
// or else!
void GLWidget::setVidShaderVars(int vidIx, bool printErrors)
{

    switch(this->vidTextures[vidIx].effect)
    {
    case VidShaderNormal:
        this->vidTextures[vidIx].shader->setUniformValue("vidTexture", 0); // texture unit index
        this->vidTextures[vidIx].shader->setUniformValue("yHeight", (GLfloat)this->vidTextures[vidIx].height);
        this->vidTextures[vidIx].shader->setUniformValue("yWidth", (GLfloat)this->vidTextures[vidIx].width);
        if(printErrors) printOpenGLError(__FILE__, __LINE__);
        break;

    case VidShaderColourHilight:
        this->vidTextures[vidIx].shader->setUniformValue("vidTexture", 0); // texture unit index
        this->vidTextures[vidIx].shader->setUniformValue("yHeight", (GLfloat)this->vidTextures[vidIx].height);
        this->vidTextures[vidIx].shader->setUniformValue("yWidth", (GLfloat)this->vidTextures[vidIx].width);
        this->vidTextures[vidIx].shader->setUniformValue("colrToDisplayMin", ColourHilightRangeMin);
        this->vidTextures[vidIx].shader->setUniformValue("colrToDisplayMax", ColourHilightRangeMax);
        if(printErrors) printOpenGLError(__FILE__, __LINE__);
        break;

    case VidShaderAlphaMask:
        this->vidTextures[vidIx].shader->setUniformValue("vidTexture", 0); // texture unit index
        this->vidTextures[vidIx].shader->setUniformValue("yHeight", (GLfloat)this->vidTextures[vidIx].height);
        this->vidTextures[vidIx].shader->setUniformValue("yWidth", (GLfloat)this->vidTextures[vidIx].width);
        this->vidTextures[vidIx].shader->setUniformValue("alphaTexture", 1); // texture unit index
        if(printErrors) printOpenGLError(__FILE__, __LINE__);
        break;

    default:
        qDebug ("Invalid effect set on vidIx %d", vidIx);
        break;
    }
}

int GLWidget::loadShaderFile(QString fileName, QString &shaderSource)
{
    shaderSource.clear();
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qCritical("File '%s' does not exist!", qPrintable(fileName));
        return -1;
    }

    QTextStream in(&file);
    while (!in.atEnd())
    {
        shaderSource += in.readLine();
        shaderSource += "\n";
    }

    return 0;
}

int GLWidget::setupShader(QGLShaderProgram *prog, QString baseFileName, bool vertNeeded, bool fragNeeded)
{
    bool ret;

    if(vertNeeded)
    {
        QString vertexShaderSource;
        ret = loadShaderFile(baseFileName+".vert", vertexShaderSource);
        if(ret != 0)
        {
            return ret;
        }

        ret = prog->addShaderFromSourceCode(QGLShader::Vertex,
                                              vertexShaderSource);
        printOpenGLError(__FILE__, __LINE__);
        if(ret == false)
        {
            qCritical() << "vertex shader log: " << prog->log();
            return -1;
        }
    }

    if(fragNeeded)
    {
        QString fragmentShaderSource;
        ret = loadShaderFile(baseFileName+".frag", fragmentShaderSource);
        if(ret != 0)
        {
            return ret;
        }

        ret = prog->addShaderFromSourceCode(QGLShader::Fragment,
                                              fragmentShaderSource);
        printOpenGLError(__FILE__, __LINE__);
        if(ret == false)
        {
            qCritical() << "fragment shader log: " << prog->log();
            return -1;
        }
    }

    ret = prog->link();
    printOpenGLError(__FILE__, __LINE__);
    if(ret == false)
    {
        qCritical() << "shader program link log: " << prog->log();
        return -1;
    }

    ret = prog->bind();
    printOpenGLError(__FILE__, __LINE__);
    if(ret == false)
    {
        return -1;
    }

    return 0;
}

int GLWidget::setupShader(QGLShaderProgram *prog, const char *shaderList[], int listLen)
{
    bool ret;

    for(int listIx = 0; listIx < listLen; listIx++)
    {
        QString shaderSource;
        ret = loadShaderFile(shaderList[listIx], shaderSource);
        if(ret != 0)
        {
            return ret;
        }

        ret = prog->addShaderFromSourceCode(QGLShader::Fragment,
                                              shaderSource);
        printOpenGLError(__FILE__, __LINE__);
        if(ret == false)
        {
            qCritical() << "vertex shader log: " << prog->log();
            return -1;
        }
    }

    ret = prog->link();
    printOpenGLError(__FILE__, __LINE__);
    if(ret == false)
    {
        qCritical() << "shader program link log: " << prog->log();
        return -1;
    }

    ret = prog->bind();
    printOpenGLError(__FILE__, __LINE__);
    if(ret == false)
    {
        return -1;
    }



    return 0;
}


int GLWidget::printOpenGLError(const char *file, int line)
{
    //
    // Returns 1 if an OpenGL error occurred, 0 otherwise.
    //
    GLenum glErr;
    int    retCode = 0;

    glErr = glGetError();
    while (glErr != GL_NO_ERROR)
    {
        qCritical() << "glError in file " << file << " @ line " << line << ": " << (const char *)gluErrorString(glErr);
        retCode = 1;
        glErr = glGetError();
    }
    return retCode;
}