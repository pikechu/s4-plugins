if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "CampaignCompletionDebug requires MSVC on Windows")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "CampaignCompletionDebug must be configured with -A Win32")
endif()
