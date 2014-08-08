#include "mayaMVG/maya/context/MVGCreateManipulator.h"
#include "mayaMVG/maya/context/MVGDrawUtil.h"
#include "mayaMVG/maya/MVGMayaUtil.h"
#include "mayaMVG/core/MVGGeometryUtil.h"
#include "mayaMVG/qt/MVGUserLog.h"

using namespace mayaMVG;

MTypeId MVGCreateManipulator::_id(0x99111); // FIXME /!\ 

MVGCreateManipulator::MVGCreateManipulator()
	: _manipUtil(NULL)
{
}

MVGCreateManipulator::~MVGCreateManipulator()
{
}

void * MVGCreateManipulator::creator()
{
	return new MVGCreateManipulator();
}

MStatus MVGCreateManipulator::initialize()
{
	return MS::kSuccess;
}

void MVGCreateManipulator::postConstructor()
{
	registerForMouseMove();
}

void MVGCreateManipulator::draw(M3dView & view, const MDagPath & path,
                               M3dView::DisplayStyle style, M3dView::DisplayStatus dispStatus)
{
	view.beginGL();

	// enable GL picking, this will call manipulator::doPress/doRelease 
	MGLuint glPickableItem;
	glFirstHandle(glPickableItem);
	colorAndName(view, glPickableItem, true, mainColor());
		
	// 3D drawing
	_manipUtil->drawPreview3D();
	
	// starts 2D drawing 
	MVGDrawUtil::begin2DDrawing(view);
	MVGDrawUtil::drawCircle(0, 0, 1, 5); // needed - FIXME

	// retrieve display data
	MVGManipulatorUtil::DisplayData* data = NULL;
	if(_manipUtil)
		data = _manipUtil->getCachedDisplayData(view);
	
	// stop drawing in case of no data or not the active view
	if(!data || !MVGMayaUtil::isActiveView(view)) {
		MVGDrawUtil::end2DDrawing();
		view.endGL();
		return;
	}
	
	// update mouse coordinates & get it in camera space
	short mousex, mousey;
	updateMouse(view, data, mousex, mousey);

	// draw 
	drawCursor(mousex, mousey);
	drawIntersections(view, mousex, mousey);
	glColor3f(1.f, 0.f, 0.f);
	drawPreview2D(view, data);
	MVGDrawUtil::end2DDrawing();
	view.endGL();
}

MStatus MVGCreateManipulator::doPress(M3dView& view)
{
	Qt::MouseButtons mouseButtons = QApplication::mouseButtons();
	if(!(mouseButtons & Qt::LeftButton))
		return MS::kFailure;
	
	MVGManipulatorUtil::DisplayData* data = NULL;
	if(_manipUtil)
		data = _manipUtil->getCachedDisplayData(view);
	if(!data)
		return MS::kFailure;
	LOG_INFO("MVGCreateManipulator::doPress")
	
	// Undo/Redo
	if(!_manipUtil->getContext()) {
	   LOG_ERROR("invalid context object.")
	   return MS::kFailure;
	}
	
	short mousex, mousey;
	MPoint mousePoint;
	mousePoint = updateMouse(view, data, mousex, mousey);

    _manipUtil->updateIntersectionState(view, data, mousex, mousey);
	switch(_manipUtil->intersectionState()) {
		case MVGManipulatorUtil::eIntersectionNone: 
        case MVGManipulatorUtil::eIntersectionPoint: {
            _createState = eCreateNone;
			data->buildPoints2D.append(mousePoint);
			// Create face if enough points (4))
			if(data->buildPoints2D.length() < 4)
				break;
			// Compute 3D face
			MPointArray facePoints3D;
			if(!MVGGeometryUtil::projectFace2D(view, facePoints3D, data->camera, data->buildPoints2D)) {
                data->buildPoints2D.remove(data->buildPoints2D.length() - 1);
                USER_ERROR("Can't find a 3D face with these points")
                break;
            }
			MDagPath emptyPath;
			if(!_manipUtil->addCreateFaceCommand(emptyPath, facePoints3D))
				return MS::kFailure;
			break;
		}
		case MVGManipulatorUtil::eIntersectionEdge: {
			_manipUtil->computeEdgeIntersectionData(view, data, mousePoint);
			_createState = eCreateExtend;
			break;
		}
	}

	return MPxManipulatorNode::doPress(view);
}

MStatus MVGCreateManipulator::doRelease(M3dView& view)
{	
	MVGManipulatorUtil::DisplayData* data = NULL;
	if(_manipUtil)
		data = _manipUtil->getCachedDisplayData(view);
	if(!data)
		return MS::kFailure;
	
	// Undo/Redo
	if(!_manipUtil->getContext()) {
	   LOG_ERROR("invalid context object.")
	   return MS::kFailure;
	}
	
	switch(_createState) {
		case eCreateNone:
			break;
		case eCreateExtend: 
		{
			MDagPath meshPath;
			MVGMayaUtil::getDagPathByName(_manipUtil->intersectionData().meshName.c_str(), meshPath);
			if(!_manipUtil->addCreateFaceCommand(meshPath, _manipUtil->previewFace3D()))
				return MS::kFailure;
			_manipUtil->previewFace3D().clear();
			break;
		}
	}
    
    // FIX ME : error with view if called after addCreateFaceCommand()
    //_manipUtil->updateIntersectionState(view, data, mousex, mousey);
    _createState = eCreateNone;	
	return MPxManipulatorNode::doRelease(view);
}

MStatus MVGCreateManipulator::doMove(M3dView& view, bool& refresh)
{	
	MVGManipulatorUtil::DisplayData* data = NULL;
	if(_manipUtil)
		data = _manipUtil->getCachedDisplayData(view);
	if(!data)
		return MS::kFailure;
	
	short mousex, mousey;
	mousePosition(mousex, mousey);
	// TODO: intersect 2D point (from camera object)
	//       or intersect 2D edge (from camera object)
	//       or intersect 3D point (fetched point from mesh object)
	_manipUtil->updateIntersectionState(view, data, mousex, mousey);
	return MPxManipulatorNode::doMove(view, refresh);
}

MStatus MVGCreateManipulator::doDrag(M3dView& view)
{
	MVGManipulatorUtil::DisplayData* data = NULL;
	if(_manipUtil)
		data = _manipUtil->getCachedDisplayData(view);
	if(!data)
		return MS::kFailure;
	
	short mousex, mousey;
	MPoint mousePoint;
	mousePoint = updateMouse(view, data, mousex, mousey);
		
	switch(_createState) {
		case eCreateNone:
			break;
		case eCreateExtend: 
		{
			computeTmpFaceOnEdgeExtend(view, data, mousePoint);
			break;
		}
	}
	return MPxManipulatorNode::doDrag(view);
}

void MVGCreateManipulator::preDrawUI(const M3dView& view)
{
}

void MVGCreateManipulator::drawUI(MHWRender::MUIDrawManager& drawManager, const MHWRender::MFrameContext&) const
{
	drawManager.beginDrawable();
	drawManager.setColor(MColor(1.0, 0.0, 0.0, 0.6));
	// TODO
	drawManager.endDrawable();
}

MPoint MVGCreateManipulator::updateMouse(M3dView& view, MVGManipulatorUtil::DisplayData* data, short& mousex, short& mousey)
{
	mousePosition(mousex, mousey);
	MPoint mousePointInCameraCoord;
	MVGGeometryUtil::viewToCamera(view, mousex, mousey, mousePointInCameraCoord);
	
	return mousePointInCameraCoord;
}

void MVGCreateManipulator::drawCursor(float mousex, float mousey)
{
	glColor3f(0.f, 0.f, 0.f);
	MVGDrawUtil::drawTargetCursor(mousex, mousey);
	
	if(_manipUtil->intersectionState() == MVGManipulatorUtil::eIntersectionEdge)
		drawExtendCursor(mousex, mousey);
}

void MVGCreateManipulator::drawExtendCursor(float mousex, float mousey)
{
	glColor3f(0.9f, 0.9f, 0.1f);
	MVGDrawUtil::drawExtendItem(mousex + 10, mousey + 10);
}

void MVGCreateManipulator::drawIntersections(M3dView& view, float mousex, float mousey)
{
	MVGManipulatorUtil::DisplayData* data = NULL;
	if(_manipUtil)
		data = _manipUtil->getCachedDisplayData(view);
	if(!data || data->allPoints2D.empty())
		return;
	
	MVGManipulatorUtil::IntersectionData& intersectionData = _manipUtil->intersectionData();
	std::vector<MVGManipulatorUtil::MVGPoint2D>& meshPoints = data->allPoints2D[intersectionData.meshName];
	
	switch(_manipUtil->intersectionState())
	{
		case MVGManipulatorUtil::eIntersectionPoint:
			break;
		case MVGManipulatorUtil::eIntersectionEdge:				
        {
            glColor4f(0.9f, 0.9f, 0.1f, 0.8f);
			glLineWidth(1.5f);
			glBegin(GL_LINES);
				MPoint point = MVGGeometryUtil::worldToView(view,  meshPoints[intersectionData.edgePointIndexes[0]].point3D);
				glVertex2f(point.x, point.y);
				point = MVGGeometryUtil::worldToView(view, meshPoints[intersectionData.edgePointIndexes[1]].point3D);
				glVertex2f(point.x, point.y);
			glEnd();	
			break;
        }
	}	
}

void MVGCreateManipulator::drawPreview2D(M3dView& view, MVGManipulatorUtil::DisplayData* data)
{
	short x, y;
	short mousex, mousey;
	mousePosition(mousex, mousey);
	
	MPointArray points = data->buildPoints2D;
	if(points.length() > 0)
	{
		for(int i = 0; i < points.length() - 1; ++i) {
			MVGGeometryUtil::cameraToView(view, points[i], x, y);
			MVGDrawUtil::drawCircle(x, y, POINT_RADIUS, 30);
			
			glLineWidth(1.5f);
			glBegin(GL_LINES);
				MVGGeometryUtil::cameraToView(view, points[i], x, y);
				glVertex2f(x, y);
				MVGGeometryUtil::cameraToView(view, points[i+1], x, y);
				glVertex2f(x, y);
			glEnd();
		}
		
		// Last point to mouse
		MVGGeometryUtil::cameraToView(view, points[points.length() - 1], x, y);
		MVGDrawUtil::drawCircle(x, y, POINT_RADIUS, 30);
		glBegin(GL_LINES);
			MVGGeometryUtil::cameraToView(view, points[points.length() - 1], x, y);
			glVertex2f(x, y);
			glVertex2f(mousex, mousey);
		glEnd();	
		
		
	}
	if(points.length() > 2)
	{
		glColor4f(0.f, 0.f, 1.f, 0.8f);
		glLineWidth(1.5f);
		glBegin(GL_LINE_LOOP);
			for(int i = 0; i < 3; ++i) {			
				MVGGeometryUtil::cameraToView(view, points[i], x, y);
				glVertex2f(x, y);
			}
			glVertex2f(mousex, mousey);
		glEnd();
		
		glColor4f(1.f, 1.f, 1.f, 0.6f);
		glBegin(GL_POLYGON);
			for(int i = 0; i < 3; ++i) {			
				MVGGeometryUtil::cameraToView(view, points[i], x, y);
				glVertex2f(x, y);
			}
			glVertex2f(mousex, mousey);
		glEnd();
	}
}

void MVGCreateManipulator::computeTmpFaceOnEdgeExtend(M3dView& view, MVGManipulatorUtil::DisplayData* data, const MPoint& mousePointInCameraCoord)
{
	MVGManipulatorUtil::IntersectionData& intersectionData = _manipUtil->intersectionData();
	std::vector<MVGManipulatorUtil::MVGPoint2D>& mvgPoints = data->allPoints2D[intersectionData.meshName];
	
	// Get edge 3D points 
	MPoint edgePoint3D_0 = mvgPoints[intersectionData.edgePointIndexes[0]].point3D;
	MPoint edgePoint3D_1 = mvgPoints[intersectionData.edgePointIndexes[1]].point3D;

	// Build 2D points preview to compute 3D face
	MPointArray previewPoints2D;
	previewPoints2D.append(mvgPoints[intersectionData.edgePointIndexes[0]].projectedPoint3D);
	previewPoints2D.append(mvgPoints[intersectionData.edgePointIndexes[1]].projectedPoint3D);
	MPoint P3 = mousePointInCameraCoord - (1 - intersectionData.edgeRatio) * intersectionData.edgeHeight2D;
	MPoint P4 = mousePointInCameraCoord + intersectionData.edgeRatio * intersectionData.edgeHeight2D;
	previewPoints2D.append(P3);
	previewPoints2D.append(P4);

	// Compute 3D face
	_manipUtil->previewFace3D().clear();
	if(!MVGGeometryUtil::projectFace2D(view, _manipUtil->previewFace3D(), data->camera, previewPoints2D, true, intersectionData.edgeHeight3D))
	{ 
        _manipUtil->previewFace3D().setLength(4);    
        MVGMesh mesh(intersectionData.meshName);
        if(!mesh.isValid())
            return;
        
        // Compute plane with old face points
        MPointArray faceVertices;
        for(int i = 0; i < intersectionData.facePointIndexes.length(); ++i)
        {
            faceVertices.append(mvgPoints[intersectionData.facePointIndexes[i]].point3D);
        }     
        MPoint movedPoint;
        PlaneKernel::Model model;
        MVGGeometryUtil::computePlane(faceVertices, model);

        // Project new points on plane		
        MVGGeometryUtil::projectPointOnPlane(P3, view, model, data->camera, movedPoint);
        _manipUtil->previewFace3D()[2] = movedPoint;

        // Keep 3D length
        MVGGeometryUtil::projectPointOnPlane(P4, view, model, data->camera, movedPoint);
        _manipUtil->previewFace3D()[3] = movedPoint;		
	}
         
    // Keep the old first 2 points to have a connected face
    _manipUtil->previewFace3D()[0] = edgePoint3D_1;
    _manipUtil->previewFace3D()[1] = edgePoint3D_0;
   
	// TODO : compute plane with straight line constraint
}
