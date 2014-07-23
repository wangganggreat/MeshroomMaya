#include "mayaMVG/maya/context/MVGManipulatorUtil.h"
#include "mayaMVG/maya/MVGMayaUtil.h"
#include "mayaMVG/maya/context/MVGDrawUtil.h"
#include "mayaMVG/core/MVGProject.h"

#include "mayaMVG/core/MVGLog.h"

namespace mayaMVG {

bool MVGManipulatorUtil::intersectPoint(M3dView& view, DisplayData* displayData, IntersectionData& intersectionData, const short&x, const short& y)
{
	if(!displayData)
		return false;
		
	double threshold = (2*POINT_RADIUS*displayData->camera.getZoom())/view.portHeight();
	MPoint mousePoint;
	MVGGeometryUtil::viewToCamera(view, displayData->camera, x, y, mousePoint);
	
	// Browse meshes
	std::map<std::string, MPointArray>& meshMap= MVGProjectWrapper::instance().getCacheMeshToPointArray();
	for(std::map<std::string, MPointArray>::iterator it = meshMap.begin(); it != meshMap.end(); ++it)
	{
		MPointArray& meshPoints = it->second;	// Points 3D
		MPoint pointCameraCoord;
		for(int i = 0; i < meshPoints.length(); ++i)
		{
			MVGGeometryUtil::worldToCamera(view, displayData->camera, meshPoints[i], pointCameraCoord);
			if(pointCameraCoord.x <= mousePoint.x + threshold && pointCameraCoord.x >= mousePoint.x - threshold
			&& pointCameraCoord.y <= mousePoint.y + threshold && pointCameraCoord.y >= mousePoint.y - threshold)
			{
				intersectionData.pointIndex = i;
				intersectionData.meshName =  it->first;

				return true;
			}
		}
	}
	
	intersectionData.pointIndex = -1;
	return false;
}

bool MVGManipulatorUtil::intersectEdge(M3dView& view, DisplayData* displayData, IntersectionData& intersectionData, const short&x, const short& y)
{
	if(!displayData)
		return false;
	
	MPoint mousePoint;
	MVGGeometryUtil::viewToCamera(view, displayData->camera, x, y, mousePoint);
	
	double minDistanceFound = -1.0;
	double tolerance = 0.001 * displayData->camera.getZoom() * 30;
	double distance;
	MIntArray tmp;
	std::string tmpMesh;
	
	// Browse meshes
	std::map<std::string, std::vector<MIntArray> >& meshMap= MVGProjectWrapper::instance().getCacheMeshToEdgeArray();
	for(std::map<std::string, std::vector<MIntArray> >::iterator it = meshMap.begin(); it != meshMap.end(); ++it)
	{
		std::vector<MIntArray>& edgesArray = it->second;
		if(edgesArray.size() < 1)
			return false;
		
		// Browse edges
		MPointArray& meshPoints = MVGProjectWrapper::instance().getMeshPoints(it->first);
		for(std::vector<MIntArray>::iterator edgeIt = edgesArray.begin(); edgeIt != edgesArray.end(); ++edgeIt)
		{
			MPoint A, B;
			MVGGeometryUtil::worldToCamera(view, displayData->camera, meshPoints[(*edgeIt)[0]], A);
			MVGGeometryUtil::worldToCamera(view, displayData->camera, meshPoints[(*edgeIt)[1]], B);
				
			MVector AB = B - A;
			MVector PA = A - mousePoint;
			MVector AP = mousePoint - A;
			MVector BP = mousePoint - B;
			MVector BA = A -B;
			// Dot signs			
			int sign1, sign2;
			((AP*AB) > 0) ? sign1 = 1 : sign1 = -1;
			((BP*BA) > 0) ? sign2 = 1 : sign2 = -1;
			if(sign1 != sign2)
				continue;
			// Lenght of orthogonal projection on edge
			double s = crossProduct2D(AB, PA) / (AB.length()*AB.length());
			if(s < 0)
				s *= -1;
			distance = s * AB.length();
			if(minDistanceFound < 0.0 || distance < minDistanceFound)
			{
				tmp.clear();
				tmp.append((*edgeIt)[0]);
				tmp.append((*edgeIt)[1]);
				tmpMesh = it->first;

				minDistanceFound = distance;
			}
		}
	}

	if(minDistanceFound < -tolerance || minDistanceFound > tolerance)
	{
		intersectionData.edgePointIndexes.clear();
		intersectionData.meshName = "";
		return false;
	}
	
	intersectionData.edgePointIndexes = tmp;
	intersectionData.meshName = tmpMesh;
	return true;
}

void MVGManipulatorUtil::drawIntersections(M3dView& view, DisplayData* data, IntersectionData& intersectionData, IntersectionState intersectionState)
{
	std::map<std::string, MPointArray>& meshCache = MVGProjectWrapper::instance().getCacheMeshToPointArray();
	//LOG_INFO("meshCache.size = " << meshCache.size());
	if(meshCache.size() > 0) {
		MPoint pointViewCoord_0;
		MPoint pointViewCoord_1;
		
		switch(intersectionState)
		{
			case MVGManipulatorUtil::eIntersectionPoint:
				glColor3f(0.f, 1.f, 0.f);
				pointViewCoord_0 = MVGGeometryUtil::worldToView(view, meshCache.at(intersectionData.meshName)[intersectionData.pointIndex]);
	
				MVGDrawUtil::drawCircle(pointViewCoord_0.x, pointViewCoord_0.y, POINT_RADIUS, 30);
				break;
			case MVGManipulatorUtil::eIntersectionEdge:				
				glColor3f(0.f, 1.f, 0.f);
				
				pointViewCoord_0 = MVGGeometryUtil::worldToView(view, meshCache.at(intersectionData.meshName)[intersectionData.edgePointIndexes[0]]);
				pointViewCoord_1 = MVGGeometryUtil::worldToView(view, meshCache.at(intersectionData.meshName)[intersectionData.edgePointIndexes[1]]);
				glBegin(GL_LINES);
					glVertex2f(pointViewCoord_0.x, pointViewCoord_0.y);
					glVertex2f(pointViewCoord_1.x, pointViewCoord_1.y);
				glEnd();	
				break;
		}	
	}
}

//void MVGManipulatorUtil::drawCameraPoints(M3dView& view, DisplayData* data)
//{
//	short x, y;
//	MPoint wPoint;
//	MVector wdir;
//	
//	// To test reloadProjectFromMaya (since map are not reload for the moment)
////	const MPointArray& points = data->cameraPoints2D;
////	for(int i = 0; i < points.length(); ++i)
////	{
////		MVGGeometryUtil::cameraToView(view, data->camera, points[i], x, y);
////		MVGDrawUtil::drawFullCross(x, y);
////	}
////	
//	for(Map3Dto2D::iterator mapIt = MVGProjectWrapper::instance().getMap3Dto2D().begin(); mapIt != MVGProjectWrapper::instance().getMap3Dto2D().end(); ++mapIt)
//	{
//		for(std::vector<PairStringToPoint>::iterator vecIt = mapIt->second.begin(); vecIt != mapIt->second.end(); ++vecIt)
//		{
//			if(data->camera.name() == vecIt->first)
//			{
//				// Draw full cross
//				MVGGeometryUtil::cameraToView(view, data->camera, vecIt->second, x, y);
//				MVGDrawUtil::drawFullCross(x, y);
//				
//				// Line toward 3D point
//				MPoint point3D_view = MVGGeometryUtil::worldToView(view, mapIt->first.second);	
//				glEnable(GL_LINE_STIPPLE);
//				glLineStipple(1.f, 0x5555);
//				glBegin(GL_LINES);
//					glVertex2f(x, y);
//					glVertex2f(point3D_view.x, point3D_view.y);
//				glEnd();
//				glDisable(GL_LINE_STIPPLE);
//				
//				view.viewToWorld(x + 5, y - 15, wPoint, wdir);
//			}
//			else
//			{
//				MPoint point3D_view =  MVGGeometryUtil::worldToView(view, mapIt->first.second);
//				MVGDrawUtil::drawEmptyCross(point3D_view.x, point3D_view.y);
//				
//				view.viewToWorld(point3D_view.x + 5, point3D_view.y - 15, wPoint, wdir);
//			}
//				
//			// Text
//			MString str;
//			str += (int)mapIt->second.size();
//			view.drawText(str, wPoint);			
//		}
//	}
//}
}	// mayaMVG