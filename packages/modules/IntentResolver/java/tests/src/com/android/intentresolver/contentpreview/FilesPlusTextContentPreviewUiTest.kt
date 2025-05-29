/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.intentresolver.contentpreview

import android.net.Uri
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.TextView
import androidx.lifecycle.testing.TestLifecycleOwner
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry.getInstrumentation
import com.android.intentresolver.R
import com.android.intentresolver.mock
import com.android.intentresolver.whenever
import com.android.intentresolver.widget.ActionRow
import com.google.common.truth.Truth.assertThat
import java.util.function.Consumer
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

private const val HEADLINE_IMAGES = "Image Headline"
private const val HEADLINE_VIDEOS = "Video Headline"
private const val HEADLINE_FILES = "Files Headline"
private const val SHARED_TEXT = "Some text to share"

@RunWith(AndroidJUnit4::class)
class FilesPlusTextContentPreviewUiTest {
    private val lifecycleOwner = TestLifecycleOwner()
    private val actionFactory =
        object : ChooserContentPreviewUi.ActionFactory {
            override fun getEditButtonRunnable(): Runnable? = null
            override fun getCopyButtonRunnable(): Runnable? = null
            override fun createCustomActions(): List<ActionRow.Action> = emptyList()
            override fun getModifyShareAction(): ActionRow.Action? = null
            override fun getExcludeSharedTextAction(): Consumer<Boolean> = Consumer<Boolean> {}
        }
    private val imageLoader = mock<ImageLoader>()
    private val headlineGenerator =
        mock<HeadlineGenerator> {
            whenever(getImagesHeadline(anyInt())).thenReturn(HEADLINE_IMAGES)
            whenever(getVideosHeadline(anyInt())).thenReturn(HEADLINE_VIDEOS)
            whenever(getFilesHeadline(anyInt())).thenReturn(HEADLINE_FILES)
        }

    private val context
        get() = getInstrumentation().getContext()

    @Test
    fun test_displayImagesPlusTextWithoutUriMetadata_showImagesHeadline() {
        val sharedFileCount = 2
        val previewView = testLoadingHeadline("image/*", sharedFileCount)

        verify(headlineGenerator, times(1)).getImagesHeadline(sharedFileCount)
        verifyPreviewHeadline(previewView, HEADLINE_IMAGES)
        verifySharedText(previewView)
    }

    @Test
    fun test_displayVideosPlusTextWithoutUriMetadata_showVideosHeadline() {
        val sharedFileCount = 2
        val previewView = testLoadingHeadline("video/*", sharedFileCount)

        verify(headlineGenerator, times(1)).getVideosHeadline(sharedFileCount)
        verifyPreviewHeadline(previewView, HEADLINE_VIDEOS)
        verifySharedText(previewView)
    }

    @Test
    fun test_displayDocsPlusTextWithoutUriMetadata_showFilesHeadline() {
        val sharedFileCount = 2
        val previewView = testLoadingHeadline("application/pdf", sharedFileCount)

        verify(headlineGenerator, times(1)).getFilesHeadline(sharedFileCount)
        verifyPreviewHeadline(previewView, HEADLINE_FILES)
        verifySharedText(previewView)
    }

    @Test
    fun test_displayMixedContentPlusTextWithoutUriMetadata_showFilesHeadline() {
        val sharedFileCount = 2
        val previewView = testLoadingHeadline("*/*", sharedFileCount)

        verify(headlineGenerator, times(1)).getFilesHeadline(sharedFileCount)
        verifyPreviewHeadline(previewView, HEADLINE_FILES)
        verifySharedText(previewView)
    }

    @Test
    fun test_displayImagesPlusTextWithUriMetadataSet_showImagesHeadline() {
        val loadedFileMetadata = createFileInfosWithMimeTypes("image/png", "image/jpeg")
        val sharedFileCount = loadedFileMetadata.size
        val previewView = testLoadingHeadline("image/*", sharedFileCount, loadedFileMetadata)

        verify(headlineGenerator, times(1)).getImagesHeadline(sharedFileCount)
        verifyPreviewHeadline(previewView, HEADLINE_IMAGES)
        verifySharedText(previewView)
    }

    @Test
    fun test_displayVideosPlusTextWithUriMetadataSet_showVideosHeadline() {
        val loadedFileMetadata = createFileInfosWithMimeTypes("video/mp4", "video/mp4")
        val sharedFileCount = loadedFileMetadata.size
        val previewView = testLoadingHeadline("video/*", sharedFileCount, loadedFileMetadata)

        verify(headlineGenerator, times(1)).getVideosHeadline(sharedFileCount)
        verifyPreviewHeadline(previewView, HEADLINE_VIDEOS)
        verifySharedText(previewView)
    }

    @Test
    fun test_displayImagesAndVideosPlusTextWithUriMetadataSet_showFilesHeadline() {
        val loadedFileMetadata = createFileInfosWithMimeTypes("image/png", "video/mp4")
        val sharedFileCount = loadedFileMetadata.size
        val previewView = testLoadingHeadline("*/*", sharedFileCount, loadedFileMetadata)

        verify(headlineGenerator, times(1)).getFilesHeadline(sharedFileCount)
        verifyPreviewHeadline(previewView, HEADLINE_FILES)
        verifySharedText(previewView)
    }

    @Test
    fun test_displayDocsPlusTextWithUriMetadataSet_showFilesHeadline() {
        val loadedFileMetadata = createFileInfosWithMimeTypes("application/pdf", "application/pdf")
        val sharedFileCount = loadedFileMetadata.size
        val previewView =
            testLoadingHeadline("application/pdf", sharedFileCount, loadedFileMetadata)

        verify(headlineGenerator, times(1)).getFilesHeadline(sharedFileCount)
        verifyPreviewHeadline(previewView, HEADLINE_FILES)
        verifySharedText(previewView)
    }

    @Test
    fun test_uriMetadataIsMoreSpecificThanIntentMimeType_headlineGetsUpdated() {
        val sharedFileCount = 2
        val testSubject =
            FilesPlusTextContentPreviewUi(
                lifecycleOwner.lifecycle,
                /*isSingleImage=*/ false,
                sharedFileCount,
                SHARED_TEXT,
                /*intentMimeType=*/ "*/*",
                actionFactory,
                imageLoader,
                DefaultMimeTypeClassifier,
                headlineGenerator
            )
        val layoutInflater = LayoutInflater.from(context)
        val gridLayout = layoutInflater.inflate(R.layout.chooser_grid, null, false) as ViewGroup

        val previewView =
            testSubject.display(context.resources, LayoutInflater.from(context), gridLayout)

        verify(headlineGenerator, times(1)).getFilesHeadline(sharedFileCount)
        verify(headlineGenerator, never()).getImagesHeadline(sharedFileCount)
        verifyPreviewHeadline(previewView, HEADLINE_FILES)

        testSubject.updatePreviewMetadata(createFileInfosWithMimeTypes("image/png", "image/jpg"))

        verify(headlineGenerator, times(1)).getFilesHeadline(sharedFileCount)
        verify(headlineGenerator, times(1)).getImagesHeadline(sharedFileCount)
        verifyPreviewHeadline(previewView, HEADLINE_IMAGES)
    }

    private fun testLoadingHeadline(
        intentMimeType: String,
        sharedFileCount: Int,
        loadedFileMetadata: List<FileInfo>? = null
    ): ViewGroup? {
        val testSubject =
            FilesPlusTextContentPreviewUi(
                lifecycleOwner.lifecycle,
                /*isSingleImage=*/ false,
                sharedFileCount,
                SHARED_TEXT,
                intentMimeType,
                actionFactory,
                imageLoader,
                DefaultMimeTypeClassifier,
                headlineGenerator
            )
        val layoutInflater = LayoutInflater.from(context)
        val gridLayout = layoutInflater.inflate(R.layout.chooser_grid, null, false) as ViewGroup

        loadedFileMetadata?.let(testSubject::updatePreviewMetadata)
        return testSubject.display(context.resources, LayoutInflater.from(context), gridLayout)
    }

    private fun createFileInfosWithMimeTypes(vararg mimeTypes: String): List<FileInfo> {
        val uri = Uri.parse("content://pkg.app/file")
        return mimeTypes.map { mimeType -> FileInfo.Builder(uri).withMimeType(mimeType).build() }
    }

    private fun verifyPreviewHeadline(previewView: ViewGroup?, expectedText: String) {
        assertThat(previewView).isNotNull()
        val headlineView = previewView?.findViewById<TextView>(R.id.headline)
        assertThat(headlineView).isNotNull()
        assertThat(headlineView?.text).isEqualTo(expectedText)
    }

    private fun verifySharedText(previewView: ViewGroup?) {
        assertThat(previewView).isNotNull()
        val textContentView = previewView?.findViewById<TextView>(R.id.content_preview_text)
        assertThat(textContentView).isNotNull()
        assertThat(textContentView?.text).isEqualTo(SHARED_TEXT)
    }
}
