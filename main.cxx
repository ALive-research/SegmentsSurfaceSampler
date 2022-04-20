// VTK includes
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

// STD includes
#include <cstdlib>

using SegmentationImageType = itk::Image<short, 3>;
using SegmentationNeighborhoodIteratorType = itk::ConstNeighborhoodIterator<SegmentationImageType>;
using SegmentationReaderType = itk::ImageFileReader<SegmentationImageType>;

int main(int argc, char **argv)
{
  //-----------------------------------------------------------------------------
  // Get the command-line arguments
  //-----------------------------------------------------------------------------
  if (argc != 4)
    {
    std::cout << "Invalid number of arguments\n";
    std::cout << "Usage:\n";
    std::cout << "./SegmentSurfaceSampler <input image> <target segment label> <output model (.vtp)>" << std::endl;
    return EXIT_FAILURE;
    }
  std::string inputFileName = argv[1];
  int targetLabel = atoi(argv[2]);
  std::string outputFileName = argv[3];

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
  long int counter = 0;
  auto sampledSurfacePoints = vtkSmartPointer<vtkPoints>::New();

  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    bool pointAdded = false;
    if(it.GetCenterPixel() == targetLabel)
      {
      for(int i=0; i<27; ++i)
        {

        // If a point was added, we don't need to continue the
        // neighborhood analysis.
        if(pointAdded)
          {
          break;
          }

        bool withinBounds;
        auto value = it.GetPixel(i, withinBounds);

        // If we are requesting a valid pixel
        if (withinBounds)
          {

          // Check for positive values different from targetLabel
          if (value != targetLabel && value > 0)
            {
            // Get the coordinates of the point and add it to the points set.
            SegmentationImageType::IndexType index = it.GetIndex(it.GetCenterNeighborhoodIndex());
            SegmentationImageType::PointType point;
            image->TransformIndexToPhysicalPoint(index, point);
            sampledSurfacePoints->InsertNextPoint(point[0], point[1], point[2]);
            counter++;
            }
          }
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

  std::cout << counter << std::endl;

  return EXIT_SUCCESS;
}
