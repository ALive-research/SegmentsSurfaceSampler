// VTK includes
#include "tclap/MultiSwitchArg.h"
#include "tclap/SwitchArg.h"
#include "tclap/ValueArg.h"
#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkTransform.h>
#include <vtkTransformFilter.h>
#include <vtkMaskPoints.h>
#include <vtkXMLPolyDataWriter.h>

// ITK includes
#include <itkImageFileReader.h>
#include <itkNeighborhoodIterator.h>

// TCLAP includes
#include <tclap/CmdLine.h>

// STD includes
#include <cstdlib>
#include <algorithm>
#include <set>

using SegmentationImageType = itk::Image<short, 3>;
using SegmentationNeighborhoodIteratorType = itk::ConstNeighborhoodIterator<SegmentationImageType>;
using SegmentationReaderType = itk::ImageFileReader<SegmentationImageType>;

int main(int argc, char **argv)
{
  std::string inputFileName ;
  std::string outputFileName;
  std::vector<int> targetLabelsV;
  int computeContour;

  try {

	TCLAP::CmdLine cmd("This program extract liver segments interfaces into clouds of points", ' ', "0.9");
  TCLAP::ValueArg<std::string> inputImageArg("i", "intput", "Path to input image file",false,"#","string");
  TCLAP::MultiArg<int> inputLabelsArg("l", "label", "Label to extract", true, "int");
  TCLAP::ValueArg<std::string> outputModelArg("o", "output", "Path to output model file", false, "#", "string");
  TCLAP::SwitchArg contourSwitch("c", "contour", "Compute only the contour", false);

  cmd.add(inputImageArg);
  cmd.add(inputLabelsArg);
  cmd.add(outputModelArg);
  cmd.add(contourSwitch);

  cmd.parse(argc, argv);

  inputFileName = inputImageArg.getValue();
  outputFileName = outputModelArg.getValue();
  targetLabelsV = inputLabelsArg.getValue();
  computeContour = contourSwitch.getValue()?1:0;

  } catch (TCLAP::ArgException &e)  // catch exceptions
    { std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl; }

  // Insert the vector values to a set
  std::set<int> targetLabels(std::make_move_iterator(targetLabelsV.begin()),
                             std::make_move_iterator(targetLabelsV.end()));

  //-----------------------------------------------------------------------------
  // Read the image (ITK)
  //-----------------------------------------------------------------------------
  SegmentationReaderType::Pointer reader = SegmentationReaderType::New();
  reader->SetFileName(inputFileName);
  reader->Update();

  SegmentationImageType::Pointer image = reader->GetOutput();

  SegmentationNeighborhoodIteratorType::RadiusType radius = {1,1,1};
  SegmentationNeighborhoodIteratorType it(radius,
                                          image,
                                          image->GetLargestPossibleRegion());

  //-----------------------------------------------------------------------------
  // Loop over the image to find segment boundaries (ITK/VTK)
  //-----------------------------------------------------------------------------
  auto sampledSurfacePoints = vtkSmartPointer<vtkPoints>::New();

  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    // Reset the condition variables.
    bool segmentInterface = false;
    bool liverInterface = !(computeContour>0);
    bool interfaceWithAnotherLabel = true;

    // if any of the target labels is the center of the window
    if(std::find(targetLabels.begin(), targetLabels.end(), it.GetCenterPixel()) != targetLabels.end())
      {
      for(int i=0; i<27; ++i)
        {
        bool withinBounds;
        auto value = it.GetPixel(i, withinBounds);

        // If we are requesting a valid pixel
        if (withinBounds)
          {
          // Check that is any of the other target labels
          bool valueInTargetLabels =
            std::find(targetLabels.begin(), targetLabels.end(), value) == targetLabels.end() && // Part of the labels set
            it.GetCenterPixel() != value;                                                       // but not the one in the center
          segmentInterface = segmentInterface || (valueInTargetLabels && value>0);
          liverInterface = liverInterface || (value==0);
          }
        }

        // Check for positive values different from targetLabel
        if (segmentInterface && liverInterface) {
          // Get the coordinates of the point and add it to the points set.
          SegmentationImageType::IndexType index = it.GetIndex(it.GetCenterNeighborhoodIndex());
          SegmentationImageType::PointType point;
          image->TransformIndexToPhysicalPoint(index, point);
          sampledSurfacePoints->InsertNextPoint(point[0], point[1], point[2]);
        }
      }
    }

  //-----------------------------------------------------------------------------
  // Assemble and transform the points (VTK)
  //-----------------------------------------------------------------------------
  auto sampledSurfacePolyData = vtkSmartPointer<vtkPolyData>::New();
  sampledSurfacePolyData->SetPoints(sampledSurfacePoints);

  auto maskPointsFilter = vtkSmartPointer<vtkMaskPoints>::New();
  maskPointsFilter->SetSingleVertexPerCell(true);
  maskPointsFilter->SetOnRatio(1);
  maskPointsFilter->SetGenerateVertices(true);
  maskPointsFilter->SetInputData(sampledSurfacePolyData);

  auto lpsToRasTransform = vtkSmartPointer<vtkTransform>::New();
  lpsToRasTransform->Scale(-1.0f, -1.0f, 1.0f);

  auto transformFilter = vtkSmartPointer<vtkTransformFilter>::New();
  transformFilter->SetTransform(lpsToRasTransform);
  transformFilter->SetInputConnection(maskPointsFilter->GetOutputPort());

  //-----------------------------------------------------------------------------
  // Write the polydata out (VTK)
  //-----------------------------------------------------------------------------
  auto xmlPolyDataWriter = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
  xmlPolyDataWriter->SetInputConnection(transformFilter->GetOutputPort());
  xmlPolyDataWriter->SetFileName(outputFileName.c_str());
  xmlPolyDataWriter->Write();

  return EXIT_SUCCESS;
}
